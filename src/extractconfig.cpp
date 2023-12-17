
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "config.h"
#include "dvbprog.h"

static std::vector<AString> configrecord;

int main(void)
{
    const auto& config = ADVBConfig::Get();
    ADVBProg prog;
    size_t i, j;

    (void)prog;

    config.SetConfigRecorder(&configrecord);

#include "config.extract.h"

    //std::sort(configrecord.begin(), configrecord.end());

    for (i = 0; i < configrecord.size(); i++) {
        for (j = i + 1; j < configrecord.size();) {
            if (configrecord[j] == configrecord[i]) {
                configrecord.erase(configrecord.begin() + j);
            }
            else j++;
        }
    }

    for (i = 0; i < configrecord.size(); i++) {
        printf("%s\n", configrecord[i].str());
    }

    return 0;
}
