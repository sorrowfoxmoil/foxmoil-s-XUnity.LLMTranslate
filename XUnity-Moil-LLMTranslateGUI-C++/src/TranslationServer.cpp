#include "TranslationServer.h"
#include "json.hpp"
#include "GlossaryManager.h" 
#include "RegexManager.h"
#include <QEventLoop>
#include <QCryptographicHash>
#include <QRegularExpression> 
#include <QRandomGenerator>
#include <regex>              
#include <chrono>
#include <QTimer> 

using json = nlohmann::json;

// ==========================================
// 📝 Server Log Dictionary
// 📝 服务器日志字典
// ==========================================
// 服务器启动日志 / Server start log
const char* SV_LOG_START[] = { "Server started. Port: %1, Threads: %2", "服务已启动，端口：%1，并发线程数：%2" };
// 服务器停止日志 / Server stop log
const char* SV_LOG_STOP[] = { "Server stopped", "服务已停止" };
// 请求接收日志 / Request received log
const char* SV_LOG_REQ[] = { "Request received: ", "收到请求: " };
// API密钥错误 / API key error
const char* SV_ERR_KEY[] = { "Error: Invalid API Key", "错误：API 密钥无效" };
// 响应格式错误 / Response format error
const char* SV_ERR_FMT[] = { "Error: Invalid Response Format", "错误：响应格式无效" };
// JSON解析错误 / JSON parse error
const char* SV_ERR_JSON[] = { "Error: JSON Parse Error", "错误：JSON 解析失败" };
// 新术语发现日志 / New term discovered log
const char* SV_NEW_TERM[] = { "✨ New Term Discovered: ", "✨ 发现新术语: " };
// 重试尝试日志 / Retry attempt log
const char* SV_RETRY_ATTEMPT[] = { "🔄 Retry translation (%1/%2): ", "🔄 重试翻译 (%1/%2): " };
// 重试成功日志 / Retry success log
const char* SV_RETRY_SUCCESS[] = { "✅ Retry successful", "✅ 重试成功" };
// 重试失败日志 / Retry failed log
const char* SV_RETRY_FAILED[] = { "❌ Retry failed, skipping text", "❌ 重试失败，跳过文本" };
// 翻译终止日志 / Translation aborted log
const char* SV_ABORTED[] = { "⛔ Translation Aborted", "⛔ 翻译已终止" };


// <实验性> 定义一个结构体来保存替换映射，确保线程安全
// 该结构体定义在 cpp 内部，作为实现细节。头文件中通过 struct EscapeMap& 前置声明引用。
struct EscapeMap {
    QMap<QString, QString> map; 
    int counter = 0; 
};

// ==========================================
// 🧊 冻结/解冻方法实现 (作为类成员函数)
// ==========================================

// 修复：从 static 改为 TranslationServer::，以匹配头文件声明
QString TranslationServer::freezeEscapesLocal(const QString& input, EscapeMap& context) {
    QString result = input;
    context.map.clear();
    context.counter = 0;
    
    // 正则：匹配 {{...}}, <...>, 以及常见的转义符
    QRegularExpression regex(R"(\{\{.*?\}\}|<[^>]+>|\\r\\n|\\n|\\r|\\t|\r\n|\n|\r|\t)");
    
    int offset = 0;
    QRegularExpressionMatchIterator i = regex.globalMatch(result);
    
    QString newResult;
    int lastEnd = 0;
    
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        
        // 1. 追加匹配项之前的内容
        newResult.append(result.mid(lastEnd, match.capturedStart() - lastEnd));
        
        // 2. 生成带空格的占位符 [T_x]
        QString original = match.captured(0);
        QString tokenKey = QString("[T_%1]").arg(context.counter++); 
        QString tokenWithSpace = QString(" %1 ").arg(tokenKey); // 前后加空格防止被LLM吞噬
        
        context.map[tokenKey] = original; // Map 中只存纯 Key
        
        newResult.append(tokenWithSpace);
        
        lastEnd = match.capturedEnd();
    }
    
    // 3. 追加剩余内容
    newResult.append(result.mid(lastEnd));
    
    return newResult;
}

// 修复：从 static 改为 TranslationServer::，以匹配头文件声明
QString TranslationServer::thawEscapesLocal(const QString& input, const EscapeMap& context) {
    QString result = input;
    
    // 正则匹配 [T_数字] 及其周围可能存在的空白字符
    QRegularExpression tokenRegex(R"(\s*\[T_(\d+)\]\s*)");
    
    QRegularExpressionMatchIterator i = tokenRegex.globalMatch(result);
    
    QString newResult;
    int lastEnd = 0;
    
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        
        // 追加前文
        newResult.append(result.mid(lastEnd, match.capturedStart() - lastEnd));
        
        // 获取 Key
        QString key = QString("[T_%1]").arg(match.captured(1));
        
        // 还原内容
        if (context.map.contains(key)) {
            newResult.append(context.map[key]);
        } else {
            // 如果找不到（极少情况），就保留 Key 原样（但去掉多余空格）
            newResult.append(key);
        }
        
        lastEnd = match.capturedEnd();
    }
    
    newResult.append(result.mid(lastEnd));
    
    return newResult;
}

// ==========================================
// 🚀 TranslationServer Implementation
// 🚀 实现
// ==========================================

TranslationServer::TranslationServer(QObject *parent) : QObject(parent), m_running(false) {
    m_stopRequested = false; 
    m_svr = nullptr; 
    m_serverThread = nullptr; 
}

TranslationServer::~TranslationServer() {
    stopServer(); 
}

void TranslationServer::updateConfig(const AppConfig& config) {
    // 🔥 同时锁定 KeyMutex 和 ConfigMutex
    std::lock_guard<std::mutex> keyLock(m_keyMutex); 
    std::lock_guard<std::mutex> cfgLock(m_configMutex); 
    
    m_config = config; 
    
    // 解析API密钥（支持逗号分隔的多个密钥）
    m_apiKeys.clear();
    QStringList keys = m_config.api_key.split(',', Qt::SkipEmptyParts);
    for(const auto& k : keys) m_apiKeys.push_back(k.trimmed());
    m_currentKeyIndex = 0; 
    
    if (m_config.enable_glossary) {
        GlossaryManager::instance().setFilePath(m_config.glossary_path);
    }
}

// 🔥 新增：线程安全地获取当前配置

AppConfig TranslationServer::getConfig() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void TranslationServer::startServer() {
    if (m_running) return; 
    m_running = true;
    m_stopRequested = false; 
    
    m_serverThread = new std::thread(&TranslationServer::runServerLoop, this);
    
    // 注意：这里读取 m_config.port 是安全的，因为 startServer 肯定是串行的（在UI线程调用）
    // 但为了严谨，读取 language 属性
    int lang = 1;
    int port = 6800;
    int threads = 1;
    {
         std::lock_guard<std::mutex> lock(m_configMutex);
         lang = m_config.language;
         port = m_config.port;
         threads = m_config.max_threads;
    }
    emit logMessage(QString(SV_LOG_START[lang]).arg(port).arg(threads));
}

void TranslationServer::stopServer() {
    if (!m_running) return; 
    
    m_stopRequested = true; 
    m_running = false;
    
    if (m_svr) m_svr->stop();
    
    if (m_serverThread && m_serverThread->joinable()) {
        m_serverThread->join();
        delete m_serverThread;
        m_serverThread = nullptr;
    }
    
    delete m_svr;
    m_svr = nullptr;
    
    // 获取语言设置用于日志
    int lang = 1;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        lang = m_config.language;
    }
    emit logMessage(SV_LOG_STOP[lang]);
}

void TranslationServer::runServerLoop() {
    m_svr = new httplib::Server(); 
    
    int threads = 1;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        threads = m_config.max_threads;
    }
    if (threads < 1) threads = 1;
    
    m_svr->new_task_queue = [threads] { return new httplib::ThreadPool(threads); };

    m_svr->Get("/",  [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("text")) { 
            res.set_content("", "text/plain"); 
            return; 
        }
        
        std::string text_std = req.get_param_value("text");
        QString text = QString::fromStdString(text_std).trimmed();
        
        if (text.isEmpty()) { 
            res.set_content("", "text/plain; charset=utf-8"); 
            return; 
        }

        // 🔥 获取语言设置需要加锁
        int langIdx = 1;
        {
             std::lock_guard<std::mutex> lock(m_configMutex);
             langIdx = m_config.language;
        }

        QString logText = text;
        logText.replace("\n", "[LF]");
        emit logMessage(QString(SV_LOG_REQ[langIdx]) + logText);
        
        emit workStarted(); 

        QString result = performTranslation(text, QString::fromStdString(req.remote_addr));
        
        if (!m_stopRequested) {
            bool success = !result.isEmpty();
            emit workFinished(success); 
        } else {
            emit workFinished(false); 
        }

        if (result.isEmpty()) {
            res.status = 500; 
            res.set_content("Translation Failed", "text/plain"); 
        } else {
            res.set_content(result.toStdString(), "text/plain; charset=utf-8");
        }
    });
    
    int port = 6800;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        port = m_config.port;
    }
    m_svr->listen("0.0.0.0", port);
}

QString TranslationServer::performTranslation(const QString& text, const QString& clientIP) {
    QString resultText = "";
    int retryCount = 0;
    const int MAX_RETRY_COUNT = 5; 
    const int RETRY_DELAY_MS = 1000; 

    // 获取当前语言配置用于日志
    int langIdx = 1;
    {
         std::lock_guard<std::mutex> lock(m_configMutex);
         langIdx = m_config.language;
    }
    
    while (retryCount < MAX_RETRY_COUNT) {
        if (m_stopRequested) {
            emit logMessage(SV_ABORTED[langIdx]);
            return "";
        }

        if (retryCount > 0) {
            QString retryMsg = QString(SV_RETRY_ATTEMPT[langIdx])
                                  .arg(retryCount + 1)
                                  .arg(MAX_RETRY_COUNT);
            emit logMessage(retryMsg);
            
            for (int i = 0; i < RETRY_DELAY_MS / 100; ++i) {
                if (m_stopRequested) return "";
                QThread::msleep(100);
            }
        }
        
        // 调用单次翻译尝试 (内部会重新获取锁来读取最新配置，确保热重载生效)
        QString attemptResult = performSingleTranslationAttempt(text, clientIP); 
        
        if (m_stopRequested) return "";

        if (isValidTranslationResult(attemptResult)) {
            if (retryCount > 0) emit logMessage(SV_RETRY_SUCCESS[langIdx]);
            resultText = attemptResult;
            break; 
        }
        
        retryCount++; 
        
        if (retryCount >= MAX_RETRY_COUNT) {
            emit logMessage(SV_RETRY_FAILED[langIdx]);
            resultText = ""; 
        }
    }
    return resultText;
}

bool TranslationServer::isValidTranslationResult(const QString& result) {
    return !result.isEmpty() && 
           !result.startsWith("Error", Qt::CaseInsensitive) &&
           !result.contains("翻译失败", Qt::CaseInsensitive) &&
           !result.contains("translation failed", Qt::CaseInsensitive) &&
           result.length() > 0;
}

QString TranslationServer::performSingleTranslationAttempt(const QString& text, const QString& clientIP) {
    if (m_stopRequested) return ""; 

    // 🔥 获取本次尝试的配置快照 (热重载核心)
    // 每次尝试时都重新读取 m_config，这样如果在重试期间用户点击了"Reload"，下一次重试就会使用新配置
    AppConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    QString apiKey = getNextApiKey();
    if (apiKey.isEmpty()) {
        emit logMessage("❌ " + QString(SV_ERR_KEY[cfg.language]));
        return "";
    }

    // ========== 第1步：局部冻结 ==========
    EscapeMap escapeCtx;
    // 使用成员函数调用
    QString processedText = freezeEscapesLocal(text, escapeCtx);
    
    if (cfg.enable_glossary) {
         processedText = RegexManager::instance().processPre(processedText);
    }

    std::string clientId = generateClientId(clientIP.toStdString()).toStdString();
    
    QString finalSystemPrompt = cfg.system_prompt;
    bool performExtraction = false; 

   finalSystemPrompt += "\n\n【Translation Rules】:\n"
                     "1. 🛑 PRESERVE TAGS: You will see tags like '[T_0]', '[T_1]'.\n"
                     "   - These replace newlines or code. Keep them EXACTLY as is.\n"
                     "   - Input: \"Hello [T_0] World\"\n"
                     "   - Output: \"你好 [T_0] 世界\"\n"
                     "2. 🛑 NO CLEANUP: Do NOT remove the tags.\n"
                     "3. 🔰 TERM CODES: Keep 'Z[A-Z]{2}Z' (e.g., 'ZMCZ') codes exactly as is.\n"
                     "4. Translate the text BETWEEN the tags naturally.\n"
                     "5. Output ONLY the translated result.\n";
                     
    if (cfg.enable_glossary) {
        QString glossaryContext = GlossaryManager::instance().getContextPrompt(processedText);
        if (!glossaryContext.isEmpty()) {
            finalSystemPrompt += "\n" + glossaryContext;
        }

        if (text.length() > 5) { 
            performExtraction = true;
            finalSystemPrompt += "\n【Term Extraction】:\n"
                                 "1. Wrap translation in <tl>...</tl>.\n"
                                 "2. If you find Proper Nouns (Names) NOT in glossary, append <tm>Src=Trgt</tm> AFTER the translation.\n" // 强调追加在后面
                                 "3. Keep <tm> tags OUTSIDE of <tl> tags.\n"; // 强调不要嵌套
        }
    }

    json messages = json::array();
    messages.push_back({{"role", "system"}, {"content", finalSystemPrompt.toStdString()}});

    {
        std::lock_guard<std::mutex> lock(m_contextMutex);
        Context& ctx = m_contexts[clientId]; 
        if (ctx.max_len != cfg.context_num) ctx.max_len = cfg.context_num;
        while (ctx.history.size() > ctx.max_len) ctx.history.pop_front();
        
        for (const auto& pair : ctx.history) {
            messages.push_back({{"role", "user"}, {"content", pair.first.toStdString()}});
            messages.push_back({{"role", "assistant"}, {"content", pair.second.toStdString()}});
        }
    }

    QString currentUserContent = cfg.pre_prompt + processedText;
    messages.push_back({{"role", "user"}, {"content", currentUserContent.toStdString()}});

    json payload;
    payload["model"] = cfg.model_name.toStdString();
    payload["messages"] = messages;
    payload["temperature"] = cfg.temperature;

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(cfg.api_address + "/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    request.setTransferTimeout(45000); 

    QNetworkReply* reply = manager.post(request, QByteArray::fromStdString(payload.dump()));
    
    QEventLoop loop;
    QTimer checkTimer;
    checkTimer.setInterval(100);
    
    QObject::connect(&checkTimer, &QTimer::timeout, [&](){
        if (m_stopRequested) {
            reply->abort(); 
            loop.quit();
        }
    });
    checkTimer.start();

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    timeoutTimer.start(40000); 
    loop.exec(); 

    QString resultText = ""; 

    if (m_stopRequested) {
        reply->deleteLater();
        return ""; 
    }

    if (!timeoutTimer.isActive()) {
        emit logMessage("❌ Request Timeout");
        reply->abort();
        reply->deleteLater();
        return ""; 
    }
    timeoutTimer.stop(); 
    checkTimer.stop(); 

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseBytes = reply->readAll();
        try {
            json response = json::parse(responseBytes.toStdString());

            if (response.contains("usage")) {
                int p = response["usage"].value("prompt_tokens", 0);
                int c = response["usage"].value("completion_tokens", 0);
                if (p > 0 || c > 0) emit tokenUsageReceived(p, c);
            }

            if (response.contains("choices") && !response["choices"].empty()) {
                std::string content = response["choices"][0]["message"]["content"];
                QString rawContent = QString::fromStdString(content);

                QString cleanContent = rawContent;
                cleanContent.remove(QRegularExpression("<think>.*?</think>", QRegularExpression::DotMatchesEverythingOption));

                if (performExtraction) {
                    QRegularExpression reTm("<tm>\\s*(.*?)\\s*=\\s*(.*?)\\s*</tm>", QRegularExpression::DotMatchesEverythingOption);
                    QRegularExpression tokenRegex(R"(\[T_\d+\])"); 
                    QRegularExpression termCodeRegex("Z[A-Z]{2}Z"); 

                    // 我们需要重构字符串，而不是简单的删除
                    // 逻辑：将 <tm>key=value</tm> 替换为 value，这样即使标签嵌在句子里，翻译也不会丢
                    QString reconstructionBuffer;
                    int lastPos = 0;
                    
                    QRegularExpressionMatchIterator i = reTm.globalMatch(cleanContent);
                    while (i.hasNext()) {
                        QRegularExpressionMatch match = i.next();
                        QString k = match.captured(1).trimmed(); // 原文
                        QString v = match.captured(2).trimmed(); // 译文
                        
                        // 1. 追加上一个匹配点到当前匹配点之间的普通文本
                        reconstructionBuffer.append(cleanContent.mid(lastPos, match.capturedStart() - lastPos));
                        
                        // 2. 处理术语逻辑
                        bool isValidTerm = true;
                        if (k.isEmpty() || v.isEmpty()) isValidTerm = false;
                        else if (k.contains(tokenRegex) || v.contains(tokenRegex)) isValidTerm = false;
                        else if (k.contains(termCodeRegex) || v.contains(termCodeRegex)) isValidTerm = false;
                        
                        if (isValidTerm) {
                            if (processedText.contains(k, Qt::CaseInsensitive)) {
                                GlossaryManager::instance().addNewTerm(k, v); 
                                emit logMessage(QString(SV_NEW_TERM[cfg.language]) + k + " = " + v);
                            }
                        }

                        // 3. 关键修复：追加“译文(v)”，而不是留空
                        // 这样 <tl>你好，<tm>Li=李</tm></tl> 就会变成 <tl>你好，李</tl>
                        // 如果标签是在外面：<tl>...</tl><tm>...</tm> -> <tl>...</tl>李 (反正后面提取tl时会忽略外面的内容，安全！)
                        reconstructionBuffer.append(v);

                        lastPos = match.capturedEnd();
                    }
                    
                    // 4. 追加剩余文本
                    reconstructionBuffer.append(cleanContent.mid(lastPos));
                    
                    // 用重构后的文本替换原文本
                    cleanContent = reconstructionBuffer;
                }

                QRegularExpression reTl("<tl>(.*?)</tl>", QRegularExpression::DotMatchesEverythingOption);
                QRegularExpressionMatch matchTl = reTl.match(cleanContent);
                
                if (matchTl.hasMatch()) {
                    resultText = matchTl.captured(1).trimmed(); 
                } else {
                    resultText = cleanContent.trimmed(); 
                }

                resultText.remove("<tl>", Qt::CaseInsensitive);
                resultText.remove("</tl>", Qt::CaseInsensitive);

                // ========== 第2步：局部解冻 ==========
                // 使用成员函数调用
                resultText = thawEscapesLocal(resultText, escapeCtx);

                if (cfg.enable_glossary) {
                    resultText = RegexManager::instance().processPost(resultText);
                }

                emit logMessage("  -> " + resultText); 

                if (isValidTranslationResult(resultText)) {
                    std::lock_guard<std::mutex> lock(m_contextMutex);
                    Context& ctx = m_contexts[clientId];
                    ctx.history.push_back({currentUserContent, resultText});
                    while (ctx.history.size() > ctx.max_len) ctx.history.pop_front();
                } else {
                    resultText = ""; 
                }
            } else {
                emit logMessage("❌ " + QString(SV_ERR_FMT[cfg.language]));
                resultText = ""; 
            }
        } catch (...) {
            emit logMessage("❌ " + QString(SV_ERR_JSON[cfg.language]));
            resultText = ""; 
        }
    } else {
        emit logMessage("❌ Network Error: " + reply->errorString());
        resultText = ""; 
    }

    reply->deleteLater(); 
    return resultText; 
}

QString TranslationServer::getNextApiKey() {
    std::lock_guard<std::mutex> lock(m_keyMutex); 
    if (m_apiKeys.empty()) return ""; 
    QString key = m_apiKeys[m_currentKeyIndex];
    m_currentKeyIndex = (m_currentKeyIndex + 1) % m_apiKeys.size(); 
    return key;
}

QString TranslationServer::generateClientId(const std::string& ip) {
    QByteArray hash = QCryptographicHash::hash(QByteArray::fromStdString(ip), QCryptographicHash::Md5);
    return hash.toHex().left(8); 
}

void TranslationServer::clearAllContexts() {
    std::lock_guard<std::mutex> lock(m_contextMutex); 
    m_contexts.clear(); 
    
    int langIdx = 1;
    {
         std::lock_guard<std::mutex> lock(m_configMutex);
         langIdx = m_config.language;
    }
    QString msg = (langIdx == 0) ? "🧹 Context memory cleared." : "🧹 上下文记忆已清空。";
    emit logMessage(msg); 
}