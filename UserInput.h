#ifndef FILEMONITOR_USERINPUT_H
#define FILEMONITOR_USERINPUT_H

#include <string>
#include <vector>

namespace UserInput
{
    int getChoice();

    std::string getPath();

    std::string getSenderEmail();
    std::string getAppPassword();
    std::vector<std::string> getRecipients();
}

#endif // FILEMONITOR_USERINPUT_H