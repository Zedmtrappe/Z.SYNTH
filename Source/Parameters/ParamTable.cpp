#include "ParamTable.h"

#include <cstring>

namespace zsynth
{
namespace
{
    constexpr ParamType Float  = ParamType::Float;
    constexpr ParamType Int    = ParamType::Int;
    constexpr ParamType Choice = ParamType::Choice;
    constexpr ParamType Bool   = ParamType::Bool;

    int countChoices(const char* list)
    {
        if (list == nullptr)
            return 0;
        int n = 1;
        for (const char* c = list; *c != 0; ++c)
            if (*c == '|')
                ++n;
        return n;
    }

    struct Table
    {
        ParamDef defs[kNumParams];

        Table()
        {
            int i = 0;
#define ZS_DEF_ENTRY(e, id, name, group, type, mn, mx, df, sc, unit, ch, help) \
            defs[i++] = ParamDef { id, name, group, type, mn, mx, df, sc, unit, ch, help };
            ZS_PARAM_LIST(ZS_DEF_ENTRY)
#undef ZS_DEF_ENTRY

            for (auto& d : defs)
            {
                if (d.type == ParamType::Choice)
                {
                    d.min = 0.0f;
                    d.max = (float) (countChoices(d.choices) - 1);
                }
            }
        }
    };

    const Table& table()
    {
        static const Table t;
        return t;
    }
} // namespace

const ParamDef& paramDef(int index) { return table().defs[index]; }

int findParamIndex(const char* id)
{
    for (int i = 0; i < kNumParams; ++i)
        if (std::strcmp(table().defs[i].id, id) == 0)
            return i;
    return -1;
}

int choiceCount(int index) { return countChoices(paramDef(index).choices); }

void choiceLabel(int index, int choice, char* buf, int bufSize)
{
    if (bufSize <= 0)
        return;
    buf[0] = 0;
    const char* c = paramDef(index).choices;
    if (c == nullptr)
        return;

    int current = 0;
    while (*c != 0 && current < choice)
        if (*c++ == '|')
            ++current;

    int n = 0;
    while (*c != 0 && *c != '|' && n < bufSize - 1)
        buf[n++] = *c++;
    buf[n] = 0;
}

void fillDefaults(float* values)
{
    for (int i = 0; i < kNumParams; ++i)
        values[i] = paramDef(i).def;
}

} // namespace zsynth
