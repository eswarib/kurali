#include "../src/TextUtils.h"
#include <cassert>
#include <string>

int main()
{
    assert(TextUtils::shouldDiscardTranscript(""));
    assert(TextUtils::shouldDiscardTranscript("   "));
    assert(TextUtils::shouldDiscardTranscript("[music]"));
    assert(TextUtils::shouldDiscardTranscript("[BLANK_AUDIO]"));
    assert(TextUtils::shouldDiscardTranscript(" [ blank audio ] "));
    assert(TextUtils::shouldDiscardTranscript("Foreign Language."));
    assert(TextUtils::shouldDiscardTranscript("music"));
    assert(TextUtils::shouldDiscardTranscript("?"));

    assert(!TextUtils::shouldDiscardTranscript("hello world"));
    assert(!TextUtils::shouldDiscardTranscript("no"));
    assert(!TextUtils::shouldDiscardTranscript("OK"));
    assert(!TextUtils::shouldDiscardTranscript("I like music"));

    return 0;
}
