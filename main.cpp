#include "FileMonitor.h"
#include "UserInput.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <conio.h>
#include <windows.h>

void printMenu()
{
    std::cout << "\n\n";
    std::cout << "Advanced Real Time File Security Monitor\n\n";

    std::cout << "1. Add / Update Path\n";
    std::cout << "2. Remove Path\n";
    std::cout << "3. Email Configuration\n";
    std::cout << "4. Start File Monitor\n";
    std::cout << "5. Exit\n";
}

void runMonitor(
    std::unique_ptr<IFileMonitor>& fm)
{
    std::cout << "\n[ Note : Press Q or q to Stop ]\n\n";

    fm->start();

    if (!fm->isRunning())
        return;

    while (fm->isRunning())
    {
        if (_kbhit())
        {
            const int key = _getch();

            if (key == 'q' || key == 'Q')
                break;
        }

        Sleep(50);
    }

    fm->stop();
}

void handleChoice(
    const int choice,
    std::unique_ptr<IFileMonitor>& fm,
    std::string& configuredPath)
{
    switch (choice)
    {
        case 1:
        {
            if (fm->isRunning())
            {
                fm->stop();
                std::cout
                    << "File Monitor stopped because the path is being changed.\n";
            }

            const std::string path = UserInput::getPath();

            if (path.empty())
            {
                std::cout << "Error: Path cannot be empty.\n";
                break;
            }

            const bool isUpdate = !configuredPath.empty();

            if (fm->addWatch(path))
            {
                configuredPath = path;

                if (isUpdate)
                {
                    std::cout << "\nPath updated successfully : "
                            << configuredPath << '\n';
                }
                else
                {
                    std::cout << "\nPath added successfully : "
                            << configuredPath << '\n';
                }
            }
            else
            {
                std::cout << "Error: Unable to add or update path.\n";
            }

            break;
        }

        case 2:
        {
            if (fm->isRunning())
                fm->stop();

            if (configuredPath.empty())
            {
                std::cout << "Error: No path has been added.\n";
                break;
            }

            if (fm->removeWatch(""))
            {
                std::cout << "Your Current Path ["
                          << configuredPath
                          << "] has been removed.\n";

                configuredPath.clear();
            }
            else
            {
                std::cout << "Error: Unable to remove the configured path.\n";
            }

            break;
        }

        case 3:
        {
            if (fm->isRunning())
            {
                std::cout
                    << "Please stop File Monitor before configuring email.\n";
                break;
            }

            try
            {
                std::cout << "\nEMAIL CONFIGURATION\n\n";

                const std::string senderEmail =
                    UserInput::getSenderEmail();

                const std::string appPassword =
                    UserInput::getAppPassword();

                const std::vector<std::string> recipients =
                    UserInput::getRecipients();

                if (!fm->configureEmailNotifications(
                        senderEmail,
                        appPassword,
                        recipients))
                {
                    std::cout
                        << "\nEmail configuration failed\n"
                        << "Error : Unable to configure email notifications.\n";
                    break;
                }

                std::cout << "\nEmail : "
                          << senderEmail << "\n\n";

                std::cout << "Password : ********\n\n";

                std::cout << "Add Recipients : ";

                for (std::size_t i = 0; i < recipients.size(); ++i)
                {
                    if (i > 0)
                        std::cout << ", ";

                    std::cout << recipients[i];
                }

                std::cout
                    << "\n\nEmail configuration completed successfully!\n";
            }
            catch (const std::exception& error)
            {
                std::cout
                    << "\nEmail configuration failed\n"
                    << "Error : " << error.what() << '\n';
            }

            break;
        }

        case 4:
        {
            runMonitor(fm);
            break;
        }

        case 5:
        {
            fm->stop();

            std::cout
                << "\nThank you for using Advanced Real Time File Security Monitor.\n";

            break;
        }

        default:
        {
            std::cout << "Invalid choice!\n";
            break;
        }
    }
}

int main()
{
    std::unique_ptr<IFileMonitor> fm =
        createFileMonitor();

    if (!fm)
    {
        std::cerr << "Failed to initialize File Monitor.\n";
        return 1;
    }

    std::string configuredPath;

    std::cout
        << "Advanced Real Time File Security Monitor\n";

    while (true)
    {
        printMenu();

        const int choice = UserInput::getChoice();

        handleChoice(choice, fm, configuredPath);

        if (choice == 5)
            break;
    }

    return 0;
}