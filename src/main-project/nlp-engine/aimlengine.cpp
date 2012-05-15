/*
 * Copyright (C) 2012 Andres Pagliano, Gabriel Miretti, Gonzalo Buteler,
 * Nestor Bustamante, Pablo Perez de Angelis
 *
 * This file is part of LVK Botmaster.
 *
 * LVK Botmaster is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LVK Botmaster is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LVK Botmaster.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "aimlengine.h"
#include "nlprule.h"
#include "nullsanitizer.h"

#include "ProgramQ/aimlparser.h"

#include <QStringList>
#include <QFile>
#include <cassert>

#define ANY_USER        "LvkNlpAimlEngineAnyUser"

//--------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------

namespace
{

// getCategoryId provide a simple way to map two numbers in one unique number
// getRuleId and getInputNumber retrieve the original values
// Asumming sizeof(long) == 8, that's true for most modern archs
// 2^12 = 4K input rules should be far enough and we have 2^52 different rules ids

const int  INPUT_NUMBER_BITS = 12; 
const long INPUT_NUMBER_MASK = 0xfffl;

//--------------------------------------------------------------------------------------------------

long getCategoryId(Lvk::Nlp::RuleId ruleId, int inputNumber)
{
    return ((long)ruleId << INPUT_NUMBER_BITS) | (long)inputNumber;
}

//--------------------------------------------------------------------------------------------------

Lvk::Nlp::RuleId getRuleId(long categoryId)
{
    return categoryId >> INPUT_NUMBER_BITS;
}

//--------------------------------------------------------------------------------------------------

int getInputNumber(long categoryId)
{
    return categoryId & INPUT_NUMBER_MASK;
}

//--------------------------------------------------------------------------------------------------

// AIML does not support our concept of "target", i.e. rules that are defined only
// for a user or group of users. In order achieve that, for each target we are enconding the
// target as a new word with form "TSTART" + Sanitized(target) + "TEND". Then we prepend this
// new word to input of the rule

QString inputWithTarget(const QString &input, const QString &target)
{
    if (target != ANY_USER) {
        return "TSTART" + QString(target).replace(QRegExp("[\\.\\?!;@  \\t]"),"X") + "TEND "
                + input;
    } else {
        return input;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// AimlEngine
//--------------------------------------------------------------------------------------------------

Lvk::Nlp::AimlEngine::AimlEngine()
    : m_aimlParser(0), m_sanitizer(new Lvk::Nlp::NullSanitizer())
{
    init();
}

//--------------------------------------------------------------------------------------------------

Lvk::Nlp::AimlEngine::AimlEngine(Sanitizer *sanitizer)
    : m_aimlParser(0), m_sanitizer(sanitizer)
{
    init();
}

//--------------------------------------------------------------------------------------------------

Lvk::Nlp::AimlEngine::~AimlEngine()
{
    delete m_sanitizer;
    delete m_aimlParser;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::init()
{
    QFile *logfile = new QFile("aiml_parser.log");
    logfile->open(QFile::WriteOnly);
    m_aimlParser = new AIMLParser(logfile);
}

//--------------------------------------------------------------------------------------------------

const Lvk::Nlp::RuleList & Lvk::Nlp::AimlEngine::rules() const
{
    return m_rules;
}

//--------------------------------------------------------------------------------------------------

Lvk::Nlp::RuleList & Lvk::Nlp::AimlEngine::rules()
{
    return m_rules;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::setRules(const Lvk::Nlp::RuleList &rules)
{
    m_rules = rules;

    QString aiml;
    buildAiml(aiml);
    m_aimlParser->loadAimlFromString(aiml);
}

//--------------------------------------------------------------------------------------------------

QString Lvk::Nlp::AimlEngine::getResponse(const QString &input, MatchList &matches)
{
    return getResponse(input, ANY_USER, matches);
}

//--------------------------------------------------------------------------------------------------

QString Lvk::Nlp::AimlEngine::getResponse(const QString &input, const QString &target,
                                          MatchList &matches)
{
    matches.clear();

    MatchList allMatches;
    QList<QString> responses = getAllResponses(input, target, allMatches);

    if (!allMatches.empty()) {
        matches.append(allMatches.first());

        assert(!responses.empty());
        return responses.first();
    } else {
        return "";
    }
}

//--------------------------------------------------------------------------------------------------

QList<QString> Lvk::Nlp::AimlEngine::getAllResponses(const QString &input, MatchList &matches)
{
    return getAllResponses(input, ANY_USER, matches);
}

//--------------------------------------------------------------------------------------------------

QList<QString> Lvk::Nlp::AimlEngine::getAllResponses(const QString &input, const QString &target,
                                                     MatchList &matches)
{
    QString szInput = m_sanitizer->sanitize(input);

    QList<long> categoriesId;
    QString response = m_aimlParser->getResponse(inputWithTarget(szInput, target), categoriesId);

    QList<QString> responses;

    if (response != "Internal Error!" && categoriesId.size() > 0) { // FIXME harcoded string
        responses.append(response);

        long catId = categoriesId.last();
        matches.append(QPair<RuleId, int>(getRuleId(catId), getInputNumber(catId)));
    } else if (target != ANY_USER) {
        // No response found with the given target, fallback to rules with any user:
        return getAllResponses(input, ANY_USER, matches);
    }

    return responses;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::buildAiml(QString &aiml)
{
    aiml = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>";
    aiml += "<aiml>";

    for (int i = 0; i < m_rules.size(); ++i) {
        buildAiml(aiml, m_rules[i]);
    }

    aiml += "</aiml>";
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::buildAiml(QString &aiml, const Rule &rule)
{
    QStringList input = m_sanitizer->sanitize(rule.input());
    const QStringList &output = rule.output();

    for (int j = 0; j < input.size(); ++j) {
        QStringList target = rule.target();

        if (target.isEmpty()) {
            target.append(ANY_USER);
        }

        for (int k = 0; k < target.size(); ++k) {
            // id is not part of AIML standar. It's an LVK extension to know which
            // rule has matched
            QString categoryId = QString::number(getCategoryId(rule.id(), j));

            aiml += "<category>";
            aiml += "<id>" + categoryId + "</id>";
            aiml += "<pattern>" + inputWithTarget(input[j], target[k]) + "</pattern>";

            if (output.size() == 1) {
                aiml += "<template>" + output[0] + "</template>";
            } else if (output.size() > 1) {
                aiml += "<template><random>";
                for (int k = 0; k < output.size(); ++k) {
                    aiml += "<li>" + output[k] + "</li>";
                }
                aiml += "</random></template>";
            }

            aiml += "</category>";
        }
    }
}
