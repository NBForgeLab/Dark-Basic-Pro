#include "DBProTools/Publisher/PublisherCli.h"

#include <iostream>
#include <string>
#include <vector>

int wmain(
    const int argumentCount,
    wchar_t* argumentValues[]) {
    std::vector<std::wstring> arguments;
    arguments.reserve(
        static_cast<std::size_t>(argumentCount));
    for (int index = 0;
         index < argumentCount;
         ++index) {
        arguments.emplace_back(argumentValues[index]);
    }
    return dbp::publisher::RunPublisherProcess(
        arguments,
        std::cout,
        std::cerr);
}
