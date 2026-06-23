#include <sourcemod>
#include <SteamWorks>

#pragma semicolon 1

/*
 * !!! CHANGE ME IF YOU ARE USING A DIFFERENT APP ID !!!
 *  OTHERWISE sm_swtest_client TESTS WILL HAVE FAILURES
 */
#define TEST_APPID   440

#define TEST_URL     "https://example.com/"
#define STREAM_URL   "https://httpbin.io/stream-bytes/16384"
#define POST_URL     "https://httpbin.io/post"

public Plugin myinfo =
{
    name        = "SteamWorks Tester",
    author      = "IRQL_NOT_LESS_OR_EQUAL",
    description = "Sanity checker for the insane",
    version     = "2026.06.23",
    url         = "https://github.com/irql-notlessorequal/SteamWorks"
};

public void OnPluginStart()
{
    RegConsoleCmd("sm_swtest",        Cmd_SWTest,       "Test SteamWorks server-side natives");
    RegConsoleCmd("sm_swtest_client", Cmd_SWTestClient, "Test SteamWorks client/authid natives (must be in-game). Usage: sm_swtest_client [groupid]");
    RegConsoleCmd("sm_swtest_http",   Cmd_SWTestHTTP,   "Test SteamWorks HTTP GET natives");
    RegConsoleCmd("sm_swtest_post",   Cmd_SWTestPOST,   "Test SteamWorks HTTP POST / raw-body natives");
    RegConsoleCmd("sm_swtest_stream", Cmd_SWTestStream, "Test SteamWorks streaming HTTP natives");
}

static void Out(int client, const char[] fmt, any ...)
{
    char buf[512];
    VFormat(buf, sizeof(buf), fmt, 3);
    ReplyToCommand(client, "[SWTest] %s", buf);
}

static void OutBool(int client, const char[] name, bool result)
{
    Out(client, "%-48s %s", name, result ? "OK" : "FAIL");
}

public Action Cmd_SWTest(int client, int args)
{
    Out(client, "=== Server Natives ===");

    OutBool(client, "SteamWorks_IsLoaded()",         SteamWorks_IsLoaded());
    OutBool(client, "SteamWorks_IsVACEnabled()",     SteamWorks_IsVACEnabled());
    OutBool(client, "SteamWorks_IsConnected()",      SteamWorks_IsConnected());

    // Public IP (array form)
    int ip[4];
    if (SteamWorks_GetPublicIP(ip))
        Out(client, "SteamWorks_GetPublicIP()                          %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    else
        Out(client, "SteamWorks_GetPublicIP()                          FAIL");

    // Public IP (cell/packed form)
    int ipCell = SteamWorks_GetPublicIPCell();
    Out(client, "SteamWorks_GetPublicIPCell()                      0x%08X", ipCell);

    // Server description fields
    OutBool(client, "SteamWorks_SetGameData(\"swtest\")",         SteamWorks_SetGameData("swtest"));
    OutBool(client, "SteamWorks_SetGameDescription(\"SW Test\")", SteamWorks_SetGameDescription("SW Test Server"));

    char map[PLATFORM_MAX_PATH];
    GetCurrentMap(map, sizeof(map));
    OutBool(client, "SteamWorks_SetMapName(<current map>)",       SteamWorks_SetMapName(map));

    // Rules
    OutBool(client, "SteamWorks_SetRule(\"sw_test\", \"1\")",    SteamWorks_SetRule("sw_test", "1"));
    OutBool(client, "SteamWorks_ClearRules()",                    SteamWorks_ClearRules());
    OutBool(client, "SteamWorks_ForceHeartbeat()",                SteamWorks_ForceHeartbeat());

    // GC -- sends a dummy message type 0; will likely fail outside a GC context
    char gcData[] = "swtest";
    EGCResults gcResult = SteamWorks_SendMessageToGC(0, gcData, sizeof(gcData));
    Out(client, "SteamWorks_SendMessageToGC(0, ...)                result=%d (0=OK, expects GC context)", gcResult);

    return Plugin_Handled;
}

public Action Cmd_SWTestClient(int client, int args)
{
    if (client == 0)
    {
        ReplyToCommand(client, "[SWTest] Must be run as an in-game client.");
        return Plugin_Handled;
    }

    Out(client, "=== Client / AuthID Natives ===");

    // SteamID string
    char steamID[64];
    SteamWorks_GetClientSteamID(client, steamID, sizeof(steamID));
    Out(client, "SteamWorks_GetClientSteamID()                    %s", steamID);

    // 32-bit Steam account ID used as authid
    int authid = GetSteamAccountID(client);
    Out(client, "authid (GetSteamAccountID)                       %d", authid);

    // License checks
    EUserHasLicenseForAppResult lic = SteamWorks_HasLicenseForApp(client, TEST_APPID);
    Out(client, "SteamWorks_HasLicenseForApp(client, %d)         result=%d (0=has, 1=no, 2=noauth)", TEST_APPID, lic);

    EUserHasLicenseForAppResult licAuth = SteamWorks_HasLicenseForAppId(authid, TEST_APPID);
    Out(client, "SteamWorks_HasLicenseForAppId(authid, %d)       result=%d", TEST_APPID, licAuth);

    // A group ID must be provided in order for these tests to run.
    int groupid = 0;
    if (args >= 1)
    {
        char arg[32];
        GetCmdArg(1, arg, sizeof(arg));
        groupid = StringToInt(arg);
    }

    if (groupid == 0)
    {
        Out(client, "SteamWorks_GetUserGroupStatus()                   SKIPPED");
        Out(client, "SteamWorks_GetUserGroupStatusAuthID()             SKIPPED");
    }
    else
    {
        OutBool(client, "SteamWorks_GetUserGroupStatus(client, groupid)",   SteamWorks_GetUserGroupStatus(client, groupid));
        OutBool(client, "SteamWorks_GetUserGroupStatusAuthID(authid, ...)", SteamWorks_GetUserGroupStatusAuthID(authid, groupid));
    }

    // Stats -- RequestStats is async; GetStat* may return false until callback fires
    OutBool(client, "SteamWorks_RequestStats(client, appid)",       SteamWorks_RequestStats(client, TEST_APPID));
    OutBool(client, "SteamWorks_RequestStatsAuthID(authid, appid)", SteamWorks_RequestStatsAuthID(authid, TEST_APPID));

    int   iVal;
    float fVal;

    bool cellOK      = SteamWorks_GetStatCell(client, "total_kills", iVal);
    bool cellAuthOK  = SteamWorks_GetStatAuthIDCell(authid, "total_kills", iVal);
    bool floatOK     = SteamWorks_GetStatFloat(client, "total_time_played", fVal);
    bool floatAuthOK = SteamWorks_GetStatAuthIDFloat(authid, "total_time_played", fVal);

    Out(client, "SteamWorks_GetStatCell()                         %s (may need RequestStats callback)", cellOK     ? "OK" : "not ready");
    Out(client, "SteamWorks_GetStatAuthIDCell()                   %s", cellAuthOK  ? "OK" : "not ready");
    Out(client, "SteamWorks_GetStatFloat()                        %s", floatOK     ? "OK" : "not ready");
    Out(client, "SteamWorks_GetStatAuthIDFloat()                  %s", floatAuthOK ? "OK" : "not ready");

    return Plugin_Handled;
}

public Action Cmd_SWTestHTTP(int client, int args)
{
    Out(client, "=== HTTP GET Natives ===");

    Handle hRequest = SteamWorks_CreateHTTPRequest(k_EHTTPMethodGET, TEST_URL);
    if (hRequest == INVALID_HANDLE)
    {
        Out(client, "SteamWorks_CreateHTTPRequest()  FAIL -- aborting");
        return Plugin_Handled;
    }
    Out(client, "SteamWorks_CreateHTTPRequest(GET, \"%s\")  OK", TEST_URL);

    // Pass caller's client index as context so the callback can use it
    OutBool(client, "SteamWorks_SetHTTPRequestContextValue()",           SteamWorks_SetHTTPRequestContextValue(hRequest, client));
    OutBool(client, "SteamWorks_SetHTTPRequestNetworkActivityTimeout()", SteamWorks_SetHTTPRequestNetworkActivityTimeout(hRequest, 10));
    OutBool(client, "SteamWorks_SetHTTPRequestHeaderValue()",            SteamWorks_SetHTTPRequestHeaderValue(hRequest, "Accept", "text/html"));
    OutBool(client, "SteamWorks_SetHTTPRequestGetOrPostParameter()",     SteamWorks_SetHTTPRequestGetOrPostParameter(hRequest, "swtest", "1"));
    OutBool(client, "SteamWorks_SetHTTPRequestUserAgentInfo()",          SteamWorks_SetHTTPRequestUserAgentInfo(hRequest, "SWTestPlugin/1.0"));
    OutBool(client, "SteamWorks_SetHTTPRequestRequiresVerifiedCertificate()", SteamWorks_SetHTTPRequestRequiresVerifiedCertificate(hRequest, false));
    OutBool(client, "SteamWorks_SetHTTPRequestAbsoluteTimeoutMS()",      SteamWorks_SetHTTPRequestAbsoluteTimeoutMS(hRequest, 5000));

    OutBool(client, "SteamWorks_SetHTTPCallbacks()",                     SteamWorks_SetHTTPCallbacks(hRequest, OnHTTPGETCompleted));

    bool sent = SteamWorks_SendHTTPRequest(hRequest);
    OutBool(client, "SteamWorks_SendHTTPRequest()",                      sent);
    if (sent)
    {
        OutBool(client, "SteamWorks_DeferHTTPRequest()",                 SteamWorks_DeferHTTPRequest(hRequest));
        OutBool(client, "SteamWorks_PrioritizeHTTPRequest()",            SteamWorks_PrioritizeHTTPRequest(hRequest));
    }

    Out(client, "(Response details will appear in server console)");
    return Plugin_Handled;
}

public void OnHTTPGETCompleted(Handle hRequest, bool bFailure, bool bRequestSuccessful, EHTTPStatusCode eStatusCode, any clientIdx)
{
    PrintToServer("[SWTest] HTTP GET callback -- Failure=%d Success=%d Status=%d", bFailure, bRequestSuccessful, eStatusCode);

    if (bFailure || !bRequestSuccessful)
    {
        CloseHandle(hRequest);
        return;
    }

    // Header
    int headerSize;
    if (SteamWorks_GetHTTPResponseHeaderSize(hRequest, "Content-Type", headerSize))
    {
        char[] hval = new char[headerSize + 1];
        SteamWorks_GetHTTPResponseHeaderValue(hRequest, "Content-Type", hval, headerSize + 1);
        PrintToServer("[SWTest] SteamWorks_GetHTTPResponseHeaderSize/Value()  Content-Type: %s", hval);
    }
    else
    {
        PrintToServer("[SWTest] SteamWorks_GetHTTPResponseHeaderSize()  FAIL");
    }

    // Body size + data
    int bodySize;
    if (SteamWorks_GetHTTPResponseBodySize(hRequest, bodySize))
    {
        PrintToServer("[SWTest] SteamWorks_GetHTTPResponseBodySize()  %d bytes", bodySize);

        char[] body = new char[bodySize + 1];
        bool dataOK = SteamWorks_GetHTTPResponseBodyData(hRequest, body, bodySize);
        PrintToServer("[SWTest] SteamWorks_GetHTTPResponseBodyData()  %s (preview: %.120s)", dataOK ? "OK" : "FAIL", body);
    }

    // Download progress (will be 1.0 at completion)
    float pct;
    SteamWorks_GetHTTPDownloadProgressPct(hRequest, pct);
    PrintToServer("[SWTest] SteamWorks_GetHTTPDownloadProgressPct()  %.4f", pct);

    // Timed out?
    bool timedOut;
    SteamWorks_GetHTTPRequestWasTimedOut(hRequest, timedOut);
    PrintToServer("[SWTest] SteamWorks_GetHTTPRequestWasTimedOut()  %d", timedOut);

    // Write body to file
    bool writeOK = SteamWorks_WriteHTTPResponseBodyToFile(hRequest, "swtest_response.html");
    PrintToServer("[SWTest] SteamWorks_WriteHTTPResponseBodyToFile()  %s", writeOK ? "OK" : "FAIL");

    // Iterate body via callback (fires synchronously)
    bool cbOK = SteamWorks_GetHTTPResponseBodyCallback(hRequest, OnHTTPBodyChunk);
    PrintToServer("[SWTest] SteamWorks_GetHTTPResponseBodyCallback()  %s", cbOK ? "OK" : "FAIL");

    CloseHandle(hRequest);
}

public void OnHTTPBodyChunk(const char[] sData, any value)
{
    PrintToServer("[SWTest] OnHTTPBodyChunk -- %d chars", strlen(sData));
}

public Action Cmd_SWTestPOST(int client, int args)
{
    Out(client, "=== HTTP POST Natives ===");

    // Raw body
    Handle hRequest = SteamWorks_CreateHTTPRequest(k_EHTTPMethodPOST, POST_URL);
    if (hRequest == INVALID_HANDLE)
    {
        Out(client, "SteamWorks_CreateHTTPRequest(POST)  FAIL");
        return Plugin_Handled;
    }
    Out(client, "SteamWorks_CreateHTTPRequest(POST, \"%s\")  OK", POST_URL);

    char body[] = "hello=world&swtest=1";
    OutBool(client, "SteamWorks_SetHTTPRequestRawPostBody()",
        SteamWorks_SetHTTPRequestRawPostBody(hRequest, "application/x-www-form-urlencoded", body, strlen(body)));

    OutBool(client, "SteamWorks_SetHTTPCallbacks(POST)",  SteamWorks_SetHTTPCallbacks(hRequest, OnHTTPPOSTCompleted));
    OutBool(client, "SteamWorks_SendHTTPRequest(POST)",   SteamWorks_SendHTTPRequest(hRequest));

    // From file (requires "swtest_body.txt" in the Path_Game directory)
    Handle hRequest2 = SteamWorks_CreateHTTPRequest(k_EHTTPMethodPOST, POST_URL);
    if (hRequest2 == INVALID_HANDLE)
    {
        Out(client, "SteamWorks_CreateHTTPRequest(POST2)  FAIL");
        return Plugin_Handled;
    }
    Out(client, "SteamWorks_CreateHTTPRequest(POST2, \"%s\")  OK", POST_URL);

    OutBool(client, "SteamWorks_SetHTTPRequestRawPostBodyFromFile()",
        SteamWorks_SetHTTPRequestRawPostBodyFromFile(hRequest2, "text/plain", "swtest_body.txt"));
    OutBool(client, "SteamWorks_SetHTTPCallbacks(POST2)", SteamWorks_SetHTTPCallbacks(hRequest2, OnHTTPPOSTCompleted));
    OutBool(client, "SteamWorks_SendHTTPRequest(POST2)",  SteamWorks_SendHTTPRequest(hRequest2));

    Out(client, "(POST response will appear in server console)");
    return Plugin_Handled;
}

public void OnHTTPPOSTCompleted(Handle hRequest, bool bFailure, bool bRequestSuccessful, EHTTPStatusCode eStatusCode, any data)
{
    PrintToServer("[SWTest] POST callback -- Failure=%d Success=%d Status=%d", bFailure, bRequestSuccessful, eStatusCode);
    CloseHandle(hRequest);
}

public Action Cmd_SWTestStream(int client, int args)
{
    Out(client, "=== Streaming HTTP Natives ===");

    Handle hRequest = SteamWorks_CreateHTTPRequest(k_EHTTPMethodGET, STREAM_URL);
    if (hRequest == INVALID_HANDLE)
    {
        Out(client, "SteamWorks_CreateHTTPRequest()  FAIL");
        return Plugin_Handled;
    }

    OutBool(client, "SteamWorks_SetHTTPCallbacks(stream, headers, data)",
        SteamWorks_SetHTTPCallbacks(hRequest, OnStreamCompleted, OnStreamHeadersReceived, OnStreamDataReceived));
    OutBool(client, "SteamWorks_SendHTTPRequestAndStreamResponse()",
        SteamWorks_SendHTTPRequestAndStreamResponse(hRequest));

    Out(client, "(Streaming chunks will appear in server console)");
    return Plugin_Handled;
}

public void OnStreamHeadersReceived(Handle hRequest, bool bFailure, any data)
{
    PrintToServer("[SWTest] OnStreamHeadersReceived -- Failure=%d", bFailure);
}

public void OnStreamDataReceived(Handle hRequest, bool bFailure, int offset, int bytesReceived)
{
    PrintToServer("[SWTest] OnStreamDataReceived -- Failure=%d Offset=%d Bytes=%d", bFailure, offset, bytesReceived);

    if (!bFailure && bytesReceived)
    {
        char[] chunk = new char[bytesReceived];
        bool ok = SteamWorks_GetHTTPStreamingResponseBodyData(hRequest, offset, chunk, bytesReceived);
        PrintToServer("[SWTest] SteamWorks_GetHTTPStreamingResponseBodyData()  %s", ok ? "OK" : "FAIL");
    }
}

public void OnStreamCompleted(Handle hRequest, bool bFailure, bool bRequestSuccessful, EHTTPStatusCode eStatusCode, any data)
{
    PrintToServer("[SWTest] Stream completed -- Failure=%d Success=%d Status=%d", bFailure, bRequestSuccessful, eStatusCode);
    CloseHandle(hRequest);
}

public SteamWorks_SteamServersConnected()
{
    PrintToServer("[SWTest] Forward: SteamWorks_SteamServersConnected()");
}

public SteamWorks_SteamServersConnectFailure(EResult result)
{
    PrintToServer("[SWTest] Forward: SteamWorks_SteamServersConnectFailure() result=%d", result);
}

public SteamWorks_SteamServersDisconnected(EResult result)
{
    PrintToServer("[SWTest] Forward: SteamWorks_SteamServersDisconnected() result=%d", result);
}

public Action SteamWorks_RestartRequested()
{
    PrintToServer("[SWTest] Forward: SteamWorks_RestartRequested()");
    return Plugin_Continue;
}

public SteamWorks_TokenRequested(char[] sToken, int maxlen)
{
    PrintToServer("[SWTest] Forward: SteamWorks_TokenRequested() token=%s", sToken);
}

public SteamWorks_OnValidateClient(int ownerauthid, int authid)
{
    PrintToServer("[SWTest] Forward: SteamWorks_OnValidateClient() owner=%d client=%d", ownerauthid, authid);
}

public SteamWorks_OnClientGroupStatus(int authid, int groupid, bool isMember, bool isOfficer)
{
    PrintToServer("[SWTest] Forward: SteamWorks_OnClientGroupStatus() authid=%d group=%d member=%d officer=%d",
        authid, groupid, isMember, isOfficer);
}