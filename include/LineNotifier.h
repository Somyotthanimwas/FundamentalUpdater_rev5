#include <string>
#ifndef LINENOTIFIER_H
#define LINENOTIFIER_H

#include <string>

class LineNotifier
{
public:

    bool Send(
        const std::string& accessToken,
        const std::string& userId,
        const std::string& message
    );

};

#endif