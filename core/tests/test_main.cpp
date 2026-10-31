#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>


int main(int argc, char** argv) {
    doctest::Context context;

    context.setOption("no-intro", true);
    context.setOption("no-version", true);
    context.setOption("case-sensitive", false);
    context.setOption("order-by", "rand");
    context.setOption("success", false);
    context.setOption("abort-after", 5);

    context.applyCommandLine(argc, argv);

    return context.run();
}
