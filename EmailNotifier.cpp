#include "EmailNotifier.h"

#include <curl/curl.h>
#include <algorithm>
#include <iostream>
#include <string>

struct EmailPayload
{
    std::string content;
    size_t position = 0;
};

static size_t payloadCallback(
    char* buffer,
    size_t size,
    size_t count,
    void* userData)
{
    auto* payload = static_cast<EmailPayload*>(userData);

    const size_t capacity = size * count;
    const size_t remaining =
        payload->content.size() - payload->position;

    const size_t bytesToCopy =
        std::min(capacity, remaining);

    if (bytesToCopy > 0)
    {
        std::copy_n(
            payload->content.data() + payload->position,
            bytesToCopy,
            buffer);

        payload->position += bytesToCopy;
    }

    return bytesToCopy;
}

EmailNotifier::EmailNotifier(
    const std::string& senderEmail,
    const std::string& appPassword,
    const std::vector<std::string>& recipients)
    : senderEmail(senderEmail),
      appPassword(appPassword),
      recipients(recipients)
{
}

bool EmailNotifier::sendNotification(
    const std::string& subject,
    const std::string& message)
{
    static const CURLcode globalInit =
        curl_global_init(CURL_GLOBAL_DEFAULT);

    if (globalInit != CURLE_OK)
    {
        std::cerr << "CURL global initialization failed.\n";
        return false;
    }

    if (recipients.empty())
    {
        std::cerr << "No email recipients configured.\n";
        return false;
    }

    CURL* curl = curl_easy_init();

    if (!curl)
    {
        std::cerr << "Failed to initialize CURL.\n";
        return false;
    }

    EmailPayload payload;

    // First recipient is the TO recipient.
    payload.content =
        "To: " + recipients[0] + "\r\n";

    // Remaining recipients are CC recipients.
    if (recipients.size() > 1)
    {
        payload.content += "Cc: ";

        for (size_t i = 1; i < recipients.size(); ++i)
        {
            if (i > 1)
                payload.content += ", ";

            payload.content += recipients[i];
        }

        payload.content += "\r\n";
    }

    payload.content +=
        "From: " + senderEmail + "\r\n" +
        "Subject: " + subject + "\r\n" +
        "\r\n" +
        message + "\r\n";

    // Add all recipients to the SMTP envelope.
    curl_slist* smtpRecipients = nullptr;

    for (const auto& recipient : recipients)
    {
        curl_slist* updatedList =
            curl_slist_append(smtpRecipients, recipient.c_str());

        if (!updatedList)
        {
            curl_slist_free_all(smtpRecipients);
            curl_easy_cleanup(curl);

            std::cerr << "Failed to configure email recipients.\n";
            return false;
        }

        smtpRecipients = updatedList;
    }

    curl_easy_setopt(
        curl, CURLOPT_URL,
        "smtp://smtp.gmail.com:587");

    curl_easy_setopt(
        curl, CURLOPT_USE_SSL,
        CURLUSESSL_ALL);

    curl_easy_setopt(
        curl, CURLOPT_USERNAME,
        senderEmail.c_str());

    curl_easy_setopt(
        curl, CURLOPT_PASSWORD,
        appPassword.c_str());

    curl_easy_setopt(
        curl, CURLOPT_MAIL_FROM,
        senderEmail.c_str());

    curl_easy_setopt(
        curl, CURLOPT_MAIL_RCPT,
        smtpRecipients);

    curl_easy_setopt(
        curl, CURLOPT_READFUNCTION,
        payloadCallback);

    curl_easy_setopt(
        curl, CURLOPT_READDATA,
        &payload);

    curl_easy_setopt(
        curl, CURLOPT_UPLOAD, 1L);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    const CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK)
    {
        std::cerr << "Email sending failed: "
                  << curl_easy_strerror(result) << '\n';
    }

    curl_slist_free_all(smtpRecipients);
    curl_easy_cleanup(curl);

    return result == CURLE_OK;
}