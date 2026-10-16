#include "PreParse.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

namespace des {

namespace {

struct Token {
    std::string text;
    enum class Kind { Word, Number, Other } kind{Kind::Other};
    std::string lower() const {
        std::string s = text;
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
};

std::vector<Token> tokenize(const std::string& s) {
    std::vector<Token> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        Token t;
        if (std::isalpha(c)) {
            t.kind = Token::Kind::Word;
            while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) t.text += s[i++];
        } else if (std::isdigit(c)) {
            t.kind = Token::Kind::Number;
            while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) ||
                                    (s[i] == '.' && i + 1 < s.size() &&
                                     std::isdigit(static_cast<unsigned char>(s[i + 1])))))
                t.text += s[i++];
        } else {
            t.text = s[i++];
        }
        out.push_back(t);
    }
    return out;
}

const std::map<std::string, int>& units() {
    static const std::map<std::string, int> m = {
        {"zero", 0}, {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}, {"five", 5},
        {"six", 6}, {"seven", 7}, {"eight", 8}, {"nine", 9}, {"ten", 10}, {"eleven", 11},
        {"twelve", 12}, {"thirteen", 13}, {"fourteen", 14}, {"fifteen", 15}, {"sixteen", 16},
        {"seventeen", 17}, {"eighteen", 18}, {"nineteen", 19}};
    return m;
}
const std::map<std::string, int>& tens() {
    static const std::map<std::string, int> m = {
        {"twenty", 20}, {"thirty", 30}, {"forty", 40}, {"fifty", 50},
        {"sixty", 60}, {"seventy", 70}, {"eighty", 80}, {"ninety", 90}};
    return m;
}

// Seconds in a unit word, or 0 when it is not one.
double unitSeconds(const std::string& w) {
    if (w == "second" || w == "seconds" || w == "sec" || w == "secs") return 1.0;
    if (w == "minute" || w == "minutes" || w == "min" || w == "mins") return 60.0;
    if (w == "hour" || w == "hours" || w == "hr" || w == "hrs")       return 3600.0;
    if (w == "day" || w == "days")                                    return 86400.0;
    return 0.0;
}

std::string num(double d) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", std::round(d * 1000.0) / 1000.0);
    return buf;
}

// Index of the next word token at or after i, or tokens.size().
std::size_t nextWord(const std::vector<Token>& t, std::size_t i) {
    while (i < t.size() && t[i].kind != Token::Kind::Word && t[i].kind != Token::Kind::Number) {
        if (t[i].text != " " && t[i].text != "-") return t.size();   // punctuation ends a phrase
        ++i;
    }
    return i;
}

// Number words -> digits. "twenty five" -> 25, "one hundred and ten" -> 110,
// "two and a half" -> 2.5, "a dozen" -> 12, "half an hour" -> 30 minutes.
std::vector<Token> numbersAsDigits(const std::vector<Token>& in) {
    std::vector<Token> out;
    auto digits = [](double v) { Token t; t.kind = Token::Kind::Number; t.text = num(v); return t; };
    auto word   = [](const std::string& s) { Token t; t.kind = Token::Kind::Word; t.text = s; return t; };
    auto space  = []() { Token t; t.text = " "; return t; };

    for (std::size_t i = 0; i < in.size();) {
        const std::string w = in[i].lower();

        // "half an hour", "half a minute", "a quarter of an hour", "quarter hour"
        if (w == "half" || w == "quarter") {
            std::size_t j = nextWord(in, i + 1);
            if (j < in.size() && in[j].lower() == "of") j = nextWord(in, j + 1);
            if (j < in.size() && (in[j].lower() == "an" || in[j].lower() == "a")) j = nextWord(in, j + 1);
            if (j < in.size() && unitSeconds(in[j].lower()) > 0.0) {
                const double secs = unitSeconds(in[j].lower()) * (w == "half" ? 0.5 : 0.25);
                // Into the next unit down, so it stays a whole number people recognise.
                if (secs >= 60.0) { out.push_back(digits(secs / 60.0)); out.push_back(space()); out.push_back(word("minutes")); }
                else              { out.push_back(digits(secs));        out.push_back(space()); out.push_back(word("seconds")); }
                i = j + 1;
                continue;
            }
        }
        if (w == "dozen") { out.push_back(digits(12)); ++i; continue; }

        // A run of number words.
        if (units().count(w) != 0 || tens().count(w) != 0) {
            double total = 0.0, current = 0.0;
            std::size_t j = i, end = i;
            while (j < in.size()) {
                const std::string x = in[j].lower();
                if (units().count(x) != 0)      current += units().at(x);
                else if (tens().count(x) != 0)  current += tens().at(x);
                else if (x == "hundred")        current = (current == 0.0 ? 1.0 : current) * 100.0;
                else if (x == "thousand")       { total += (current == 0.0 ? 1.0 : current) * 1000.0; current = 0.0; }
                else if (x == "and") {
                    // "and" continues a number after hundred or thousand --
                    // "one hundred and ten" -- or before "a half". Anywhere
                    // else it is a list: "two and three" is two numbers.
                    const std::string before = in[end - 1].lower();
                    const bool joins = before == "hundred" || before == "thousand";
                    const std::size_t k = nextWord(in, j + 1);
                    if (k >= in.size()) break;
                    const std::string y = in[k].lower();
                    if (y == "a") {
                        const std::size_t h = nextWord(in, k + 1);
                        if (h < in.size() && in[h].lower() == "half") {
                            current += 0.5;
                            end = h + 1;
                            j = h + 1;
                            break;
                        }
                        break;
                    }
                    if (!joins || (units().count(y) == 0 && tens().count(y) == 0)) break;
                } else {
                    break;
                }
                end = j + 1;
                j = nextWord(in, j + 1);
                if (j >= in.size()) break;
            }
            out.push_back(digits(total + current));
            i = end;
            continue;
        }

        // "2 and a half" with digits.
        if (in[i].kind == Token::Kind::Number) {
            std::size_t a = nextWord(in, i + 1);
            if (a < in.size() && in[a].lower() == "and") {
                std::size_t b = nextWord(in, a + 1);
                std::size_t h = b < in.size() ? nextWord(in, b + 1) : in.size();
                if (b < in.size() && in[b].lower() == "a" && h < in.size() && in[h].lower() == "half") {
                    out.push_back(digits(std::atof(in[i].text.c_str()) + 0.5));
                    i = h + 1;
                    continue;
                }
            }
        }
        out.push_back(in[i]);
        ++i;
    }
    return out;
}

bool isRateWord(const std::string& w) {
    return w == "per" || w == "an" || w == "a" || w == "each" || w == "every";
}

}  // namespace

PreParsed preParse(const std::string& description) {
    std::vector<Token> t = numbersAsDigits(tokenize(description));
    PreParsed out;

    // "<n> [up to three words] per|an|a|each|every <unit>" is a RATE. The words
    // between may not include a time unit: "5 minutes per customer" is a time.
    std::vector<std::pair<std::size_t, std::string>> inserts;   // after token i
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (t[i].kind != Token::Kind::Number) continue;
        std::size_t j = nextWord(t, i + 1);
        for (int words = 0; j < t.size() && words <= 3; ++words) {
            const std::string w = t[j].lower();
            if (unitSeconds(w) > 0.0) break;
            if (isRateWord(w)) {
                const std::size_t u = nextWord(t, j + 1);
                if (u >= t.size()) break;
                const double per = unitSeconds(t[u].lower());
                const double n = std::atof(t[i].text.c_str());
                if (per > 0.0 && n > 0.0) {
                    double every = per / n;                 // seconds between arrivals
                    std::string unit = "seconds";
                    if (every >= 3600.0 * 2) { every /= 3600.0; unit = "hours"; }
                    else if (every >= 60.0)  { every /= 60.0;   unit = "minutes"; }
                    std::string phrase;
                    for (std::size_t k = i; k <= u; ++k) phrase += t[k].text;
                    const std::string note = phrase + " = one every " + num(every) + " " + unit;
                    out.notes.push_back(note);
                    // "on average": a bare "(one every 2 minutes)" reads as a
                    // timetable, and a model given it chose fixed gaps.
                    inserts.emplace_back(u, " (on average one every " + num(every) + " " + unit + ")");
                }
                break;
            }
            if (t[j].kind == Token::Kind::Number) break;
            j = nextWord(t, j + 1);
        }
    }

    // "about 15 minutes": Qwen 3.8 27B, told that "about" means the pattern
    // "around", still wrote "random" -- exponential, the most variable choice
    // there is, so an "about 15 minute" visit often took 2 or 45. The hint
    // goes beside the words, where the rate hint already worked.
    for (std::size_t i = 0; i < t.size(); ++i) {
        const std::string w = t[i].lower();
        if (w != "about" && w != "around" && w != "roughly" && w != "approximately" &&
            w != "approx" && w != "usually" && w != "typically")
            continue;
        const std::size_t n = nextWord(t, i + 1);
        if (n >= t.size() || t[n].kind != Token::Kind::Number) continue;
        const std::size_t u = nextWord(t, n + 1);
        if (u >= t.size() || unitSeconds(t[u].lower()) <= 0.0) continue;
        out.notes.push_back(t[i].text + " " + t[n].text + " " + t[u].text + " = around " + t[n].text +
                            ", varying a little either side");
        inserts.emplace_back(u, " (pattern \"around\", mean " + t[n].text + ")");
    }
    std::sort(inserts.begin(), inserts.end(),
              [](const std::pair<std::size_t, std::string>& a, const std::pair<std::size_t, std::string>& b) {
                  return a.first < b.first;
              });

    std::size_t next = 0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        out.text += t[i].text;
        while (next < inserts.size() && inserts[next].first == i) out.text += inserts[next++].second;
    }
    return out;
}

}  // namespace des
