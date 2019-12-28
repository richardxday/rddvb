
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/strsup.h>

#include <json/json.h>
#include <curl/curl.h>
#include <openssl/sha.h>

static size_t __writedata(void *contents, size_t size, size_t count, void *context)
{
    std::string& res = *(std::string *)context;
    size_t bytes = size * count;

    res += std::string((const char *)contents, bytes);

    return bytes;
}

static bool Post(CURL *curl, const char *url, const Json::Value& postdata, Json::Value& resp)
{
    struct curl_slist *slist = NULL;
    std::string _postdata, _response;
    CURLcode res;
    bool success = false;

    {
        Json::FastWriter writer;
        _postdata = writer.write(postdata);
    }

    slist = curl_slist_append(slist, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _postdata.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.38.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __writedata);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &_response);

    res = curl_easy_perform(curl);

    curl_slist_free_all(slist);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }
    else {
        Json::Reader reader;
        if (!reader.parse(_response.c_str(), resp)) {
            fprintf(stderr, "Failed to parse response '%s'\n", _response.c_str());
        }
        else success = true;
    }

    return success;
}

void SetAuthentication(Json::Value& obj, const char *user, const char *password)
{
    SHA_CTX ctx;
    uint8_t hash[SHA_DIGEST_LENGTH];
    char    sha1password[SHA_DIGEST_LENGTH * 2 + 1];

    SHA1_Init(&ctx);
    SHA1_Update(&ctx, password, strlen(password));
    SHA1_Final(hash, &ctx);

    uint_t i;
    for (i = 0; i < NUMBEROF(hash); i++) sprintf(sha1password + i * 2, "%02x", (uint_t)hash[i]);

    obj["username"] = user;
    obj["password"] = sha1password;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    CURL *curl;
    if ((curl = curl_easy_init()) != NULL) {
        Json::Value postdata, resp;

        SetAuthentication(postdata, "richardxday", "yessometvforme");

        if (Post(curl, "https://json.schedulesdirect.org/20141201/token", postdata, resp)) {
            printf("Response: %s\n", resp.toStyledString().c_str());

            int code = -1;
            if (resp.isMember("code") && resp["code"].isInt() && ((code = resp["code"].asInt()) == 0) &&
                resp.isMember("token") && resp["token"].isString()) {
                std::string token = resp["token"].asString();


            }
            else fprintf(stderr, "Response reported error (code %d)\n", code);
        }

        curl_easy_cleanup(curl);
    }

    return 0;
}
