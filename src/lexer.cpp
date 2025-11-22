#include "lexer.hpp"
#include <iostream>
#include <sstream>

std::vector<std::pair<Token, std::string>> Lexer::lexString(std::string s) {
    // std::cerr << s << "\n";
    std::vector<std::pair<Token, std::string>> ans;
    size_t i = 0;
    while (i < s.size()) {
        int bestlen = 0;
        std::pair<Token, std::string> bestmatch;
        if (s.substr(i, 2) == "//") {
            uint32_t cur = 0;
            while (s[i + cur] != '\n' && s[i + cur] != '\r') {
                cur++;
            }
            ans.push_back({Token::kCOMMENT, s.substr(i, cur)});
            i += cur;
            continue;
        }
        if (s.substr(i, 2) == "/*") {
            uint32_t cur = 2, cnt = 1;
            while (cnt > 0) {
                if (s.substr(i + cur, 2) == "/*") {
                    cnt++;
                } else if (s.substr(i + cur, 2) == "*/") {
                    cnt--;
                }
                cur++;
            }
            ans.push_back({Token::kCOMMENT, s.substr(i, cur + 2)});
            i += cur + 2;
            continue;
        }
        const auto &patterns = (('a' <= s[i] && s[i] <= 'z') || ('A' <= s[i] && s[i] <= 'Z')) ? letterpatterns : nonletterpatterns;
        for (size_t j = 0; j < patterns.size(); j++) {
            auto tokentype = patterns[j].first;
            auto regexrule = patterns[j].second;

            boost::smatch match;
            auto sub = s.substr(i);  // 从当前位置开始截取
            if (boost::regex_search(sub, match, regexrule) && match.position() == 0) {
                auto matchstr = match.str();
                if (matchstr.size() > bestlen) {
                    bestlen = matchstr.size();
                    bestmatch = std::pair<Token, std::string>{tokentype, matchstr};
                }
            }
        }
        
        if (bestlen > 0) {
            if (bestmatch.first != Token::kCOMMENT) {
                ans.push_back(bestmatch);
            }
            i += bestlen - 1;
        }
        i++;
    }

    // for (size_t i = 0; i < ans.size(); i++) {
    //     auto p = ans[i];
    //     std::cerr << i << " " << to_string(p.first) << " " << p.second << "\n";
    // }

    return ans;
}

std::string Lexer::GetString() {
    std::string ans;

    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    ans = buffer.str();

    return ans;
}