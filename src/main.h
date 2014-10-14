/*!
 * \file main.h
 * \brief
 * Provides basic macros that are not specific to any single part of the code
 */
#ifndef MAIN_H
#define MAIN_H

/*!
 * \brief
 * Marks a variable as intentionally unused
 *
 * Intended to both give the developer information about variable usage, and
 * get rid of compiler warnings about unused variables and arguments. Modern
 * compilers should be able to expand this macro without generating any extra
 * code.
 */
#define UNUSED(x) (void)(x)

/*!
 * \brief
 * Helper macro to allow macro expansions, don't use this directly
 *
 * \sa #STR(x)
 */
#define STR_HELPER(x) #x
/*!
 * \brief
 * Converts an integer constant to a string using the preprocessor
 */
#define STR(x) STR_HELPER(x)

#endif /* MAIN_H */
