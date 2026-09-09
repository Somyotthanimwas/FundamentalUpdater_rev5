#include "LineNotifier.h"

#include <curl/curl.h>

bool LineNotifier::Send(
    const std::string& accessToken,
    const std::string& userId,
    const std::string& message)
{
    CURL* curl = curl_easy_init();

    if(!curl)
        return false;

    struct curl_slist* headers = nullptr;

    headers = curl_slist_append(
        headers,
        ("Authorization: Bearer " + accessToken).c_str());

    headers = curl_slist_append(
        headers,
        "Content-Type: application/json");

    std::string json =
        "{"
        "\"to\":\"" + userId + "\","
        "\"messages\":["
        "{"
        "\"type\":\"text\","
        "\"text\":\"" + message + "\""
        "}"
        "]"
        "}";

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        "https://api.line.me/v2/bot/message/push");

    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers);

    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDS,
        json.c_str());

    CURLcode res =
        curl_easy_perform(curl);

    curl_slist_free_all(headers);

    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}