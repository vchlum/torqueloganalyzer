#ifndef FASTLiNEREADER_H
#define FASTLiNEREADER_H

// STD C++
#include <iosfwd>

/** Quick line-by-line parser of text files
 *
 *  This function provides a fast line parser with a callback model.
 *
 * @param filename file to be parsed
 * @param callback function that will be called for each line
 * @returns 0 on success, -1 if file could not be opened
 **/
int fastLineParser(const char * const filename, void (*callback)(const char * const, const char * const));

#endif // FASTLiNEREADER_H
