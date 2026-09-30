#include "ape_analyze.h"

int data[16];

#if defined(NO_ROOT)
/** @brief Export an access without creating an analysis root. @return Nothing. */
void unselected(void) { data[0] = 1; }
#elif defined(OPAQUE)
/** @brief A known task is not an inline helper. @return Nothing. */
APE_ANALYZE void opaque(void) { data[0] = 1; }
/** @brief Preserve a forbidden task-to-task call in strict MAP. @return Nothing. */
APE_ANALYZE void selected(void) { opaque(); }
#elif defined(LATER_FAILURE)
/** @brief Emit earlier valid work before the later failure. @return Nothing. */
APE_ANALYZE void first(void) { data[0] = 1; }
/** @brief Leave the reached index unresolved.
 * @param index Unknown runtime value.
 * @return Nothing.
 */
APE_ANALYZE void second(int index) { data[index] = 2; }
#elif defined(SKIPPED_BODY)
/** @brief Do not resolve sources in an unexecuted body.
 * @param index Unknown runtime value used only in the skipped body.
 * @return Nothing.
 */
APE_ANALYZE void selected(int index)
{
  for (int i = 0; i < 0; ++i) data[index] = 1;
  data[0] = 1;
}
#else
#error "select a rejection case"
#endif

/** @brief Link the selected fixture without executing its tasks. @return Zero. */
int main(void) { return 0; }
