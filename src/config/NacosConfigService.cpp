#include "src/config/NacosConfigService.h"
#include "src/http/HTTPCli.h"
#include "src/http/ServerHttpAgent.h"
#include "Constants.h"
#include "Parameters.h"
#include "utils/ParamUtils.h"
#include "Debug.h"
#include "src/md5/md5.h"

using namespace std;

namespace nacos{
NacosConfigService::NacosConfigService
        (
                AppConfigManager *_appConfigManager,
                HTTPCli *_httpCli,
                ServerListManager *_serverListManager,
                HttpAgent *_httpAgent,
                ClientWorker *_clientWorker
        ) throw(NacosException) {
    appConfigManager = _appConfigManager;
    httpcli = _httpCli;
    svrListMgr = _serverListManager;
    httpAgent = _httpAgent;
    clientWorker = _clientWorker;
}

NacosConfigService::~NacosConfigService() {
    log_debug("NacosConfigService::~NacosConfigService()\n");
    if (clientWorker != NULL) {
        clientWorker->stopListening();
        delete clientWorker;
        clientWorker = NULL;
    }

    if (httpAgent != NULL) {
        delete httpAgent;
        httpAgent = NULL;
    }

    if (svrListMgr != NULL) {
        delete svrListMgr;
        svrListMgr = NULL;
    }

    if (httpcli != NULL) {
        delete httpcli;
        httpcli = NULL;
    }

    if (appConfigManager != NULL) {
        delete appConfigManager;
        appConfigManager = NULL;
    }
}

NacosString NacosConfigService::getConfig
        (
                const NacosString &dataId,
                const NacosString &group,
                long timeoutMs
        ) throw(NacosException) {
    return getConfigInner(getNamespace(), dataId, group, timeoutMs);
}

bool NacosConfigService::publishConfig
        (
                const NacosString &dataId,
                const NacosString &group,
                const NacosString &content
        ) throw(NacosException) {
    return publishConfigInner(getNamespace(), dataId, group, NULLSTR, NULLSTR, NULLSTR, content);
}

bool NacosConfigService::removeConfig
        (
                const NacosString &dataId,
                const NacosString &group
        ) throw(NacosException) {
    return removeConfigInner(getNamespace(), dataId, group, NULLSTR);
}

NacosString NacosConfigService::getConfigInner
        (
                const NacosString &tenant,
                const NacosString &dataId,
                const NacosString &group,
                long timeoutMs
        ) throw(NacosException) {
    return clientWorker->getServerConfig(tenant, dataId, group, timeoutMs);
}

bool NacosConfigService::removeConfigInner
        (
                const NacosString &tenant,
                const NacosString &dataId,
                const NacosString &group,
                const NacosString &tag
        ) throw(NacosException) {
    std::list <NacosString> headers;
    std::list <NacosString> paramValues;
    //Get the request url
    NacosString url = DEFAULT_CONTEXT_PATH + Constants::CONFIG_CONTROLLER_PATH;

    HttpResult res;

    paramValues.push_back("dataId");
    paramValues.push_back(dataId);

    NacosString parmGroupid = ParamUtils::null2defaultGroup(group);
    paramValues.push_back("group");
    paramValues.push_back(parmGroupid);

    if (!isNull(tenant)) {
        paramValues.push_back("tenant");
        paramValues.push_back(tenant);
    }

    try {
        res = httpAgent->httpDelete(url, headers, paramValues, httpAgent->getEncode(), POST_TIMEOUT);
    }
    catch (NetworkException e) {
        log_warn("[remove] error, %s, %s, %s, msg: %s\n", dataId.c_str(), group.c_str(), tenant.c_str(), e.what());
        return false;
    }

    //If the server returns true, then this call succeeds
    if (res.content.compare("true") == 0) {
        return true;
    } else {
        return false;
    }
}

bool NacosConfigService::publishConfigInner
        (
                const NacosString &tenant,
                const NacosString &dataId,
                const NacosString &group,
                const NacosString &tag,
                const NacosString &appName,
                const NacosString &betaIps,
                const NacosString &content
        ) throw(NacosException) {
    //TODO:More stringent check, need to improve checkParam() function
    ParamUtils::checkParam(dataId, group, content);

    std::list <NacosString> headers;
    std::list <NacosString> paramValues;
    NacosString parmGroupid;
    //Get the request url
    NacosString url = DEFAULT_CONTEXT_PATH + Constants::CONFIG_CONTROLLER_PATH;

    HttpResult res;

    parmGroupid = ParamUtils::null2defaultGroup(group);
    paramValues.push_back("group");
    paramValues.push_back(parmGroupid);

    paramValues.push_back("dataId");
    paramValues.push_back(dataId);

    paramValues.push_back("content");
    paramValues.push_back(content);

    if (!isNull(tenant)) {
        paramValues.push_back("tenant");
        paramValues.push_back(tenant);
    }

    if (!isNull(appName)) {
        paramValues.push_back("appName");
        paramValues.push_back(appName);
    }

    if (!isNull(tag)) {
        paramValues.push_back("tag");
        paramValues.push_back(tag);
    }

    if (!isNull(betaIps)) {
        headers.push_back("betaIps");
        headers.push_back(betaIps);
    }

    try {
        res = httpAgent->httpPost(url, headers, paramValues, httpAgent->getEncode(), POST_TIMEOUT);
    }
    catch (NetworkException e) {
        //
        log_warn("[{}] [publish-single] exception, dataId=%s, group=%s, msg=%s\n", dataId.c_str(), group.c_str(),
                 tenant.c_str(), e.what());
        return false;
    }

    //If the server returns true, then this call succeeds
    if (res.content.compare("true") == 0) {
        return true;
    } else {
        return false;
    }
}

void NacosConfigService::addListener
        (
                const NacosString &dataId,
                const NacosString &group,
                Listener *listener
        ) throw(NacosException) {
    NacosString parmgroup = Constants::DEFAULT_GROUP;
    if (!isNull(group)) {
        parmgroup = group;
    }

    //TODO:give a constant to this hard-coded number
    NacosString cfgcontent = getConfig(dataId, group, 3000);

    clientWorker->addListener(dataId, parmgroup, getNamespace(), cfgcontent, listener);
    clientWorker->startListening();
}

void NacosConfigService::removeListener
        (
                const NacosString &dataId,
                const NacosString &group,
                Listener *listener
        ) {
    NacosString parmgroup = Constants::DEFAULT_GROUP;
    if (!isNull(group)) {
        parmgroup = group;
    }
    log_debug("NacosConfigService::removeListener()\n");
    clientWorker->removeListener(dataId, parmgroup, getNamespace(), listener);
}

}//namespace nacos
