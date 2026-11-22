#include "UserInput.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
    std::string trim(const std::string& value)
    {
        const auto start = value.find_first_not_of(" \t\r\n");

        if (start == std::string::npos)
            return "";

        const auto end = value.find_last_not_of(" \t\r\n");

        return value.substr(start, end - start + 1);
    }

    bool isValidEmail(const std::string& email)
    {
        const auto atPosition = email.find('@');

        if (atPosition == std::string::npos || atPosition == 0)
            return false;

        if (email.find('@', atPosition + 1) != std::string::npos)
            return false;

        const auto dotPosition = email.find('.', atPosition + 1);

        if (dotPosition == std::string::npos ||
            dotPosition == atPosition + 1 ||
            dotPosition == email.length() - 1)
            return false;

        if (email.find(' ') != std::string::npos)
            return false;

        return true;
    }

    std::string trimPath(const std::string& path)
    {
        std::string result = trim(path);

        if (result.size() >= 2 &&
            result.front() == '"' && result.back() == '"')
        {
            result = result.substr(1, result.size() - 2);
        }

        return result;
    }

    std::string convertToPlatformPath(std::string path)
    {
#ifdef _WIN32
        return path;
#else
        if (path.size() >= 3 &&
            std::isalpha(static_cast<unsigned char>(path[0])) &&
            path[1] == ':' &&
            (path[2] == '\\' || path[2] == '/'))
        {
            char drive = static_cast<char>(
                std::tolower(static_cast<unsigned char>(path[0])));

            path = "/mnt/" + std::string(1, drive) + path.substr(2);
        }

        std::replace(path.begin(), path.end(), '\\', '/');

        return path;
#endif
    }

    std::string readMaskedPassword()
    {
        std::string password;

        std::cout << "Password : " << std::flush;

#ifdef _WIN32

        while (true)
        {
            const int ch = _getch();

            if (ch == '\r')
                break;

            if (ch == '\b')
            {
                if (!password.empty())
                {
                    password.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            }
            else if (ch >= 32 && ch <= 126)
            {
                password.push_back(static_cast<char>(ch));
                std::cout << '*' << std::flush;
            }
        }

#else

        termios oldSettings{};
        termios newSettings{};

        if (tcgetattr(STDIN_FILENO, &oldSettings) != 0)
            throw std::runtime_error(
                "Unable to enable password masking.");

        newSettings = oldSettings;
        newSettings.c_lflag &= ~ECHO;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &newSettings) != 0)
            throw std::runtime_error(
                "Unable to enable password masking.");

        while (true)
        {
            const int ch = getchar();

            if (ch == '\n' || ch == EOF)
                break;

            if (ch == 127 || ch == '\b')
            {
                if (!password.empty())
                {
                    password.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            }
            else if (ch >= 32 && ch <= 126)
            {
                password.push_back(static_cast<char>(ch));
                std::cout << '*' << std::flush;
            }
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);

#endif

        std::cout << '\n';

        // Gmail App Passwords may be copied with spaces.
        password.erase(
            std::remove_if(
                password.begin(),
                password.end(),
                [](unsigned char c)
                {
                    return std::isspace(c);
                }),
            password.end());

        return password;
    }
}

int UserInput::getChoice()
{
    int choice{};

    while (true)
    {
        std::cout << "Enter command (1-5): ";

        std::cin >> choice;

        if (!std::cin)
        {
            if (std::cin.eof())
                std::exit(0);

            std::cout << "Invalid input!\n";

            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');

            continue;
        }

        std::cin.ignore(
            std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice >= 1 && choice <= 5)
            return choice;

        std::cout << "Invalid choice! Please enter a number from 1 to 5.\n";
    }
}

std::string UserInput::getPath()
{
    std::string path;

    std::cout << "Path : ";
    std::getline(std::cin, path);

    path = trimPath(path);

    if (path.empty())
        return "";

    return convertToPlatformPath(path);
}

std::string UserInput::getSenderEmail()
{
    std::string email;

    std::cout << "Email : ";
    std::getline(std::cin, email);

    email = trim(email);

    if (email.empty())
        throw std::runtime_error("Email address is required.");

    if (!isValidEmail(email))
        throw std::runtime_error("Please enter a valid sender email.");

    return email;
}

std::string UserInput::getAppPassword()
{
    std::string password = readMaskedPassword();

    if (password.empty())
        throw std::runtime_error("App Password is required.");

    return password;
}

std::vector<std::string> UserInput::getRecipients()
{
    std::string input;

    std::cout
        << "Add Recipients : "
        << "recipient:to@gmail.com, "
        << "recipient:cc1@gmail.com, "
        << "recipient:cc2@gmail.com\n";

    std::cout << "                 [ Note : Up to 3 emails allowed ]\n";

    std::cout << "Enter recipients: ";
    std::getline(std::cin, input);

    input = trim(input);

    if (input.empty())
        throw std::runtime_error(
            "At least one recipient email is required.");

    std::stringstream ss(input);
    std::string email;
    std::vector<std::string> recipients;

    while (std::getline(ss, email, ','))
    {
        email = trim(email);

        if (email.empty() || !isValidEmail(email))
            throw std::runtime_error(
                "Please enter valid recipient email addresses.");

        recipients.push_back(email);

        if (recipients.size() > 3)
            throw std::runtime_error(
                "Maximum 3 recipient emails are allowed.");
    }

    if (recipients.empty())
        throw std::runtime_error(
            "At least one recipient email is required.");

    return recipients;
}