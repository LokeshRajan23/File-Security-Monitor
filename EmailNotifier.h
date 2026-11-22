#ifndef EMAIL_NOTIFIER_H
#define EMAIL_NOTIFIER_H

#include <string>
#include <vector>

class EmailNotifier
{
public:
    EmailNotifier(
        const std::string& senderEmail,
        const std::string& appPassword,
        const std::vector<std::string>& recipients
    );

    bool sendNotification(
        const std::string& subject,
        const std::string& message
    );

private:
    std::string senderEmail;
    std::string appPassword;

    // First recipient is TO; remaining recipients are CC.
    std::vector<std::string> recipients;
};

#endif // EMAIL_NOTIFIER_H