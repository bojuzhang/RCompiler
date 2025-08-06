#include "lexser.hpp"

std::vector<std::pair<Token, std::string>> lexser::lexString(std::string s) {
    std::vector<std::pair<Token, std::string>> ans;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isspace(s[i])) ++i;

        int bestlen = 0;
        std::pair<Token, std::string> bestmatch;
        for (size_t j = 0; j < patterns.size(); j++) {
            auto tokentype = patterns[j].first;
            auto regexrule = patterns[j].second;

            std::smatch match;
            auto sub = s.substr(i);  // 从当前位置开始截取
            if (std::regex_search(sub, match, regexrule) && match.position() == 0) {
                auto matchstr = match.str();
                if (matchstr.size() > bestlen) {
                    bestlen = matchstr.size();
                    bestmatch = std::pair<Token, std::string>{tokentype, matchstr};
                }
            }
        }

        ans.push_back(bestmatch);
    }

    return ans;
}