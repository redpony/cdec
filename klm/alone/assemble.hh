#ifndef ALONE_ASSEMBLE__
#define ALONE_ASSEMBLE__

#include <iosfwd>

namespace search {
class Final;
} // namespace search

namespace alone {

std::ostream &operator<<(std::ostream &o, const search::Final &final);

void DetailedFinal(std::ostream &o, const search::Final &final, const char *indent_str = "  ");

// This isn't called anywhere but makes it easy to print from gdb.
void PrintFinal(const search::Final &final);

} // namespace alone

#endif // ALONE_ASSEMBLE__
