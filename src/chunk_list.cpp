/**
 * @file chunk_list.cpp
 * Manages and navigates the list of chunks.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "chunk_list.h"

#include "ListManager.h"
#include "prototypes.h"
#include "space.h"

typedef ListManager<chunk_t> ChunkList_t;


/**
 * use this enum to define in what direction or location an
 * operation shall be performed.
 */
enum class direction_e : unsigned int
{
   FORWARD,
   BACKWARD
};


/**
 * @brief prototype for a function that checks a chunk to have a given type
 *
 * @note this typedef defines the function type "check_t"
 * for a function pointer of type
 * bool function(chunk_t *pc)
 */
typedef bool (*check_t)(chunk_t *pc);


/**
 * @brief prototype for a function that searches through a chunk list
 *
 * @note this typedef defines the function type "search_t"
 * for a function pointer of type
 * chunk_t *function(chunk_t *cur, nav_t scope)
 */
typedef chunk_t * (*search_t)(chunk_t *cur, scope_e scope);


/**
 * @brief search for a chunk that satisfies a condition in a chunk list
 *
 * A generic function that traverses a chunks list either
 * in forward or reverse direction. The traversal continues until a
 * chunk satisfies the condition defined by the compare function.
 * Depending on the parameter cond the condition will either be
 * checked to be true or false.
 *
 * Whenever a chunk list traversal is to be performed this function
 * shall be used. This keeps the code clear and easy to understand.
 *
 * If there are performance issues this function might be worth to
 * be optimized as it is heavily used.
 *
 * @param  cur        chunk to start search at
 * @param  check_fct  compare function
 * @param  scope      code parts to consider for search
 * @param  dir        search direction
 * @param  cond       success condition
 *
 * @retval nullptr  no requested chunk was found or invalid parameters provided
 * @retval chunk_t  pointer to the found chunk
 */
static chunk_t *chunk_search(chunk_t *cur, const check_t check_fct, const scope_e scope = scope_e::ALL, const direction_e dir = direction_e::FORWARD, const bool cond = true);


/**
 * @brief search for a chunk that satisfies a condition in a chunk list.
 *
 * This function is similar to chunk_search, except that it is tweaked to
 * handle searches inside of preprocessor directives. Specifically, if the
 * starting token is inside a preprocessor directive, it will ignore a line
 * continuation, and will abort the search if it reaches the end of the
 * directive. This function only searches forward.
 *
 * @param  cur        chunk to start search at
 * @param  check_fct  compare function
 * @param  scope      code parts to consider for search
 * @param  cond       success condition
 *
 * @retval nullptr  no requested chunk was found or invalid parameters provided
 * @retval chunk_t  pointer to the found chunk or pointer to the chunk at the
 *                  end of the preprocessor directive
 */
static chunk_t *chunk_ppa_search(chunk_t *cur, const check_t check_fct, const bool cond = true);


static void chunk_log(chunk_t *pc, const char *text);


/*
 * TODO: if we use C++ we can overload the following two functions
 * and thus name them equally
 */

/**
 * @brief search a chunk of a given category in a chunk list
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * This function is a specialization of chunk_search.
 *
 * @param cur    chunk to start search at
 * @param type   category to search for
 * @param scope  code parts to consider for search
 * @param dir    search direction
 *
 * @retval nullptr  no chunk found or invalid parameters provided
 * @retval chunk_t  pointer to the found chunk
 */
static chunk_t *chunk_search_type(chunk_t *cur, const c_token_t type, const scope_e scope = scope_e::ALL, const direction_e dir = direction_e::FORWARD);


/**
 * @brief search a chunk of a given type and level
 *
 * Traverses a chunk list in the specified direction until a chunk of a given type
 * is found.
 *
 * This function is a specialization of chunk_search.
 *
 * @param cur    chunk to start search at
 * @param type   category to search for
 * @param scope  code parts to consider for search
 * @param dir    search direction
 * @param level  nesting level to match or -1 / ANY_LEVEL
 *
 * @retval nullptr  no chunk found or invalid parameters provided
 * @retval chunk_t  pointer to the found chunk
 */
static chunk_t *chunk_search_type_level(chunk_t *cur, c_token_t type, scope_e scope = scope_e::ALL, direction_e dir = direction_e::FORWARD, int level = -1);


/**
 * @brief searches a chunk that holds a specific string
 *
 * Traverses a chunk list either in forward or backward direction until a chunk
 * with the provided string was found. Additionally a nesting level can be
 * provided to narrow down the search.
 *
 * @param  cur    chunk to start search at
 * @param  str    string that searched chunk needs to have
 * @param  len    length of the string
 * @param  scope  code parts to consider for search
 * @param  dir    search direction
 * @param  level  nesting level of the searched chunk, ignored when negative
 *
 * @retval NULL     no chunk found or invalid parameters provided
 * @retval chunk_t  pointer to the found chunk
 */
static chunk_t *chunk_search_str(chunk_t *cur, const char *str, size_t len, scope_e scope, direction_e dir, int level);


/**
 * @brief Add a new chunk before/after the given position in a chunk list
 *
 * If ref is nullptr, add either at the head or tail based on the specified pos
 *
 * @param  pc_in  chunk to add to list
 * @param  ref    insert position in list
 * @param  pos    insert before or after
 *
 * @return chunk_t  pointer to the added chunk
 */
static chunk_t *chunk_add(const chunk_t *pc_in, chunk_t *ref, const direction_e pos = direction_e::FORWARD);


/**
 * @brief Determines which chunk search function to use
 *
 * Depending on the required search direction return a pointer
 * to the corresponding chunk search function.
 *
 * @param dir  search direction
 *
 * @return pointer to chunk search function
 */
static search_t select_search_fct(const direction_e dir = direction_e::FORWARD);


ChunkList_t g_cl; //! global chunk list


chunk_t *chunk_get_head(void)
{
   return(g_cl.GetHead());
}


chunk_t *chunk_get_tail(void)
{
   return(g_cl.GetTail());
}


static search_t select_search_fct(const direction_e dir)
{
   return((dir == direction_e::FORWARD) ? chunk_get_next : chunk_get_prev);
}


chunk_t *chunk_search_prev_cat(chunk_t *pc, const c_token_t cat)
{
   return(chunk_search_type(pc, cat, scope_e::ALL, direction_e::BACKWARD));
}


chunk_t *chunk_search_next_cat(chunk_t *pc, const c_token_t cat)
{
   return(chunk_search_type(pc, cat, scope_e::ALL, direction_e::FORWARD));
}


bool are_chunks_in_same_line(chunk_t *start, chunk_t *end)
{
   chunk_t *tmp;

   if (start != nullptr)
   {
      tmp = chunk_get_next(start);
   }
   else
   {
      return(false);
   }

   while (  tmp != nullptr
         && tmp != end)
   {
      if (chunk_is_token(tmp, CT_NEWLINE))
      {
         return(false);
      }
      tmp = chunk_get_next(tmp);
   }
   return(true);
}


static chunk_t *chunk_search_type(chunk_t *cur, const c_token_t type,
                                  const scope_e scope, const direction_e dir)
{
   /*
    * Depending on the parameter dir the search function searches
    * in forward or backward direction
    */
   search_t search_function = select_search_fct(dir);
   chunk_t  *pc             = cur;

   do                                  // loop over the chunk list
   {
      pc = search_function(pc, scope); // in either direction while
   } while (  pc != nullptr            // the end of the list was not reached yet
           && pc->type != type);       // and the demanded chunk was not found either

   return(pc);                         // the latest chunk is the searched one
}


static chunk_t *chunk_search_type_level(chunk_t *cur, c_token_t type, scope_e scope, direction_e dir, int level)
{
   /*
    * Depending on the parameter dir the search function searches
    * in forward or backward direction
    */
   search_t search_function = select_search_fct(dir);
   chunk_t  *pc             = cur;

   do                                  // loop over the chunk list
   {
      pc = search_function(pc, scope); // in either direction while
   } while (  pc != nullptr            // the end of the list was not reached yet
           && (!is_expected_type_and_level(pc, type, level)));

   return(pc);                         // the latest chunk is the searched one
}


static chunk_t *chunk_search_str(chunk_t *cur, const char *str, size_t len, scope_e scope, direction_e dir, int level)
{
   /*
    * Depending on the parameter dir the search function searches
    * in forward or backward direction */
   search_t search_function = select_search_fct(dir);
   chunk_t  *pc             = cur;

   do                                  // loop over the chunk list
   {
      pc = search_function(pc, scope); // in either direction while
   } while (  pc != nullptr            // the end of the list was not reached yet
           && (!is_expected_string_and_level(pc, str, level, len)));

   return(pc);                         // the latest chunk is the searched one
}


static chunk_t *chunk_search(chunk_t *cur, const check_t check_fct, const scope_e scope,
                             const direction_e dir, const bool cond)
{
   /*
    * Depending on the parameter dir the search function searches
    * in forward or backward direction */
   search_t search_function = select_search_fct(dir);
   chunk_t  *pc             = cur;

   do                                   // loop over the chunk list
   {
      pc = search_function(pc, scope);  // in either direction while
   } while (  pc != nullptr             // the end of the list was not reached yet
           && (check_fct(pc) != cond)); // and the demanded chunk was not found either

   return(pc);                          // the latest chunk is the searched one
}


static chunk_t *chunk_ppa_search(chunk_t *cur, const check_t check_fct, const bool cond)
{
   if (  cur != nullptr
      && !cur->flags.test(PCF_IN_PREPROC))
   {
      // if not in preprocessor, do a regular search
      return(chunk_search(cur, check_fct, scope_e::ALL,
                          direction_e::FORWARD, cond));
   }
   chunk_t *pc = cur;

   while (  pc != nullptr
         && (pc = pc->next) != nullptr)
   {
      if (!pc->flags.test(PCF_IN_PREPROC))
      {
         // Bail if we run off the end of the preprocessor directive, but
         // return the next token, NOT nullptr, because the caller may need to
         // know where the search ended
         assert(chunk_is_token(pc, CT_NEWLINE));
         return(pc);
      }

      if (chunk_is_token(pc, CT_NL_CONT))
      {
         // Skip line continuation
         continue;
      }

      if (check_fct(pc) == cond)
      {
         // Requested token was found
         return(pc);
      }
   }
   // Ran out of tokens
   return(nullptr);
}


/* @todo maybe it is better to combine chunk_get_next and chunk_get_prev
 * into a common function However this should be done with the preprocessor
 * to avoid addition check conditions that would be evaluated in the
 * while loop of the calling function */
chunk_t *chunk_get_next(chunk_t *cur, scope_e scope)
{
   if (cur == nullptr)
   {
      return(nullptr);
   }
   chunk_t *pc = g_cl.GetNext(cur);

   if (  pc == nullptr
      || scope == scope_e::ALL)
   {
      return(pc);
   }

   if (cur->flags.test(PCF_IN_PREPROC))
   {
      // If in a preproc, return nullptr if trying to leave
      if (!pc->flags.test(PCF_IN_PREPROC))
      {
         return(nullptr);
      }
      return(pc);
   }

   // Not in a preproc, skip any preproc
   while (  pc != nullptr
         && pc->flags.test(PCF_IN_PREPROC))
   {
      pc = g_cl.GetNext(pc);
   }
   return(pc);
}


chunk_t *chunk_get_prev(chunk_t *cur, scope_e scope)
{
   if (cur == nullptr)
   {
      return(nullptr);
   }
   chunk_t *pc = g_cl.GetPrev(cur);

   if (  pc == nullptr
      || scope == scope_e::ALL)
   {
      return(pc);
   }

   if (cur->flags.test(PCF_IN_PREPROC))
   {
      // If in a preproc, return NULL if trying to leave
      if (!pc->flags.test(PCF_IN_PREPROC))
      {
         return(nullptr);
      }
      return(pc);
   }

   // Not in a preproc, skip any preproc
   while (  pc != nullptr
         && pc->flags.test(PCF_IN_PREPROC))
   {
      pc = g_cl.GetPrev(pc);
   }
   return(pc);
}


chunk_t *chunk_dup(const chunk_t *pc_in)
{
   chunk_t *pc = new chunk_t; // Allocate a new chunk

   if (pc == nullptr)
   {
      // @todo clean up properly before crashing
      LOG_FMT(LERR, "Failed to allocate memory\n");
      log_func_stack_inline(LSETFLG);
      log_flush(true);
      exit(EXIT_FAILURE);
   }
   // Copy all fields and then init the entry
   *pc = *pc_in; // TODO: what happens if pc_in == nullptr?
   g_cl.InitEntry(pc);

   return(pc);
}


static void chunk_log_msg(chunk_t *chunk, const log_sev_t log, const char *str)
{
   LOG_FMT(log, "%s orig_line is %zu, orig_col is %zu, ",
           str, chunk->orig_line, chunk->orig_col);

   if (chunk_is_token(chunk, CT_NEWLINE))
   {
      LOG_FMT(log, "<Newline>,\n");
   }
   else if (chunk_is_token(chunk, CT_VBRACE_OPEN))
   {
      LOG_FMT(log, "<VBRACE_OPEN>,\n");
   }
   else if (chunk_is_token(chunk, CT_VBRACE_CLOSE))
   {
      LOG_FMT(log, "<VBRACE_CLOSE>,\n");
   }
   else
   {
      LOG_FMT(log, "text() is '%s', type is %s,\n", chunk->text(), get_token_name(chunk->type));
   }
}


static void chunk_log(chunk_t *pc, const char *text)
{
   if (  pc != nullptr
      && (cpd.unc_stage != unc_stage_e::TOKENIZE)
      && (cpd.unc_stage != unc_stage_e::CLEANUP))
   {
      const log_sev_t log   = LCHUNK;
      chunk_t         *prev = chunk_get_prev(pc);
      chunk_t         *next = chunk_get_next(pc);

      chunk_log_msg(pc, log, text);

      if (  prev != nullptr
         && next != nullptr)
      {
         chunk_log_msg(prev, log, "   @ between");
         chunk_log_msg(next, log, "   and");
      }
      else if (next != nullptr)
      {
         chunk_log_msg(next, log, "   @ before");
      }
      else if (prev != nullptr)
      {
         chunk_log_msg(prev, log, "   @ after");
      }
      LOG_FMT(log, "   stage is %s",                             // Issue #3034
              get_unc_stage_name(cpd.unc_stage));
      log_func_stack_inline(log);
   }
}


chunk_t *chunk_add_after(const chunk_t *pc_in, chunk_t *ref)
{
   return(chunk_add(pc_in, ref, direction_e::FORWARD));
}


chunk_t *chunk_add_before(const chunk_t *pc_in, chunk_t *ref)
{
   return(chunk_add(pc_in, ref, direction_e::BACKWARD));
}


void chunk_del(chunk_t * &pc)
{
   g_cl.Pop(pc);
   delete pc;
   pc = nullptr;
}


void chunk_move_after(chunk_t *pc_in, chunk_t *ref)
{
   LOG_FUNC_ENTRY();
   g_cl.Pop(pc_in);
   g_cl.AddAfter(pc_in, ref);

   // HACK: Adjust the original column
   pc_in->column       = ref->column + space_col_align(ref, pc_in);
   pc_in->orig_col     = pc_in->column;
   pc_in->orig_col_end = pc_in->orig_col + pc_in->len();
}


chunk_t *chunk_get_next_nl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_newline, scope, direction_e::FORWARD, true));
}


chunk_t *chunk_get_prev_nl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_newline, scope, direction_e::BACKWARD, true));
}


chunk_t *chunk_get_next_nc(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment, scope, direction_e::FORWARD, false));
}


chunk_t *chunk_get_prev_nc(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment, scope, direction_e::BACKWARD, false));
}


chunk_t *chunk_get_next_nnl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_newline, scope, direction_e::FORWARD, false));
}


chunk_t *chunk_get_prev_nnl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_newline, scope, direction_e::BACKWARD, false));
}


chunk_t *chunk_get_next_nc_nnl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_or_newline, scope, direction_e::FORWARD, false));
}


chunk_t *chunk_get_prev_nc_nnl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_or_newline, scope, direction_e::BACKWARD, false));
}


chunk_t *chunk_get_next_nc_nnl_np(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_newline_or_preproc, scope, direction_e::FORWARD, false));
}


chunk_t *chunk_get_prev_nc_nnl_np(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_newline_or_preproc, scope, direction_e::BACKWARD, false));
}


chunk_t *chunk_get_next_nc_nnl_in_pp(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_or_newline_in_preproc, scope, direction_e::FORWARD, false));
}


chunk_t *chunk_get_prev_nc_nnl_in_pp(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_or_newline_in_preproc, scope, direction_e::BACKWARD, false));
}


chunk_t *chunk_ppa_get_next_nc_nnl(chunk_t *cur)
{
   return(chunk_ppa_search(cur, chunk_is_comment_or_newline, false));
}


chunk_t *chunk_get_next_nc_nnl_nb(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_newline_or_blank, scope, direction_e::FORWARD, false));
}


chunk_t *chunk_get_prev_nc_nnl_nb(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_newline_or_blank, scope, direction_e::BACKWARD, false));
}


chunk_t *chunk_get_next_nisq(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_balanced_square, scope, direction_e::FORWARD, false));
}


chunk_t *chunk_get_prev_nc_nnl_ni(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_or_newline_or_ignored, scope, direction_e::BACKWARD, false));
}


chunk_t *chunk_get_next_type(chunk_t *cur, c_token_t type, int level, scope_e scope)
{
   return(chunk_search_type_level(cur, type, scope, direction_e::FORWARD, level));
}


chunk_t *chunk_get_prev_type(chunk_t *cur, c_token_t type, int level, scope_e scope)
{
   return(chunk_search_type_level(cur, type, scope, direction_e::BACKWARD, level));
}


chunk_t *chunk_get_next_str(chunk_t *cur, const char *str, size_t len, int level, scope_e scope)
{
   return(chunk_search_str(cur, str, len, scope, direction_e::FORWARD, level));
}


chunk_t *chunk_get_prev_str(chunk_t *cur, const char *str, size_t len, int level, scope_e scope)
{
   return(chunk_search_str(cur, str, len, scope, direction_e::BACKWARD, level));
}


bool chunk_is_newline_between(chunk_t *start, chunk_t *end)
{
   for (chunk_t *pc = start; pc != end; pc = chunk_get_next(pc))
   {
      if (chunk_is_newline(pc))
      {
         return(true);
      }
   }

   return(false);
}


void chunk_swap(chunk_t *pc1, chunk_t *pc2)
{
   g_cl.Swap(pc1, pc2);
}


// TODO: the following function shall be made similar to the search functions
chunk_t *chunk_first_on_line(chunk_t *pc)
{
   chunk_t *first = pc;

   while (  (pc = chunk_get_prev(pc)) != nullptr
         && !chunk_is_newline(pc))
   {
      first = pc;
   }
   return(first);
}


bool chunk_is_last_on_line(chunk_t &pc)  //TODO: pc should be const here
{
   // check if pc is the very last chunk of the file
   const auto *end = chunk_get_tail();

   if (&pc == end)
   {
      return(true);
   }
   // if the next chunk is a newline then pc is the last chunk on its line
   const auto *next = chunk_get_next(&pc);

   if (chunk_is_token(next, CT_NEWLINE))
   {
      return(true);
   }
   return(false);
}


// TODO: this function needs some cleanup
void chunk_swap_lines(chunk_t *pc1, chunk_t *pc2)
{
   // to swap lines we need to find the first chunk of the lines
   pc1 = chunk_first_on_line(pc1);
   pc2 = chunk_first_on_line(pc2);

   if (  pc1 == nullptr
      || pc2 == nullptr
      || pc1 == pc2)
   {
      return;
   }
   /*
    * Example start:
    * ? - start1 - a1 - b1 - nl1 - ? - ref2 - start2 - a2 - b2 - nl2 - ?
    *      ^- pc1                              ^- pc2
    */
   chunk_t *ref2 = chunk_get_prev(pc2);

   // Move the line started at pc2 before pc1
   while (  pc2 != nullptr
         && !chunk_is_newline(pc2))
   {
      chunk_t *tmp = chunk_get_next(pc2);
      g_cl.Pop(pc2);
      g_cl.AddBefore(pc2, pc1);
      pc2 = tmp;
   }
   /*
    * Should now be:
    * ? - start2 - a2 - b2 - start1 - a1 - b1 - nl1 - ? - ref2 - nl2 - ?
    *                         ^- pc1                              ^- pc2
    */

   // Now move the line started at pc1 after ref2
   while (  pc1 != nullptr
         && !chunk_is_newline(pc1))
   {
      chunk_t *tmp = chunk_get_next(pc1);
      g_cl.Pop(pc1);

      if (ref2 != nullptr)
      {
         g_cl.AddAfter(pc1, ref2);
      }
      else
      {
         g_cl.AddHead(pc1);
      }
      ref2 = pc1;
      pc1  = tmp;
   }
   /*
    * Should now be:
    * ? - start2 - a2 - b2 - nl1 - ? - ref2 - start1 - a1 - b1 - nl2 - ?
    *                         ^- pc1                              ^- pc2
    */

   /*
    * pc1 and pc2 should be the newlines for their lines.
    * swap the chunks and the nl_count so that the spacing remains the same.
    */
   if (  pc1 != nullptr
      && pc2 != nullptr)
   {
      size_t nl_count = pc1->nl_count;

      pc1->nl_count = pc2->nl_count;
      pc2->nl_count = nl_count;

      chunk_swap(pc1, pc2);
   }
} // chunk_swap_lines


chunk_t *chunk_get_next_nvb(chunk_t *cur, const scope_e scope)
{
   return(chunk_search(cur, chunk_is_vbrace, scope, direction_e::FORWARD, false));
}


chunk_t *chunk_get_prev_nvb(chunk_t *cur, const scope_e scope)
{
   return(chunk_search(cur, chunk_is_vbrace, scope, direction_e::BACKWARD, false));
}


void chunk_flags_set_real(chunk_t *pc, pcf_flags_t clr_bits, pcf_flags_t set_bits)
{
   if (pc != nullptr)
   {
      LOG_FUNC_ENTRY();
      auto const nflags = (pc->flags & ~clr_bits) | set_bits;

      if (pc->flags != nflags)
      {
         LOG_FMT(LSETFLG,
                 "%s(%d): %016llx^%016llx=%016llx\n"
                 "   orig_line is %zu, orig_col is %zu, text() '%s', type is %s,",
                 __func__, __LINE__,
                 static_cast<pcf_flags_t::int_t>(pc->flags),
                 static_cast<pcf_flags_t::int_t>(pc->flags ^ nflags),
                 static_cast<pcf_flags_t::int_t>(nflags),
                 pc->orig_line, pc->orig_col, pc->text(),
                 get_token_name(pc->type));
         LOG_FMT(LSETFLG, " parent_type is %s,\n  ",
                 get_token_name(get_chunk_parent_type(pc)));
         log_func_stack_inline(LSETFLG);
         pc->flags = nflags;
      }
   }
}


void set_chunk_type_real(chunk_t *pc, c_token_t token, const char *func, int line)
{
   LOG_FUNC_ENTRY();

   if (  pc == nullptr
      || pc->type == token)
   {
      return;
   }
   LOG_FMT(LSETTYP, "%s(%d): orig_line is %zu, orig_col is %zu, pc->text() ",
           func, line, pc->orig_line, pc->orig_col);

   if (token == CT_NEWLINE)
   {
      LOG_FMT(LSETTYP, "<Newline>\n");
   }
   else
   {
      LOG_FMT(LSETTYP, "'%s'\n", pc->text());
   }
   LOG_FMT(LSETTYP, "   pc->type is %s, pc->parent_type is %s => *type is %s, *parent_type is %s\n",
           get_token_name(pc->type), get_token_name(get_chunk_parent_type(pc)),
           get_token_name(token), get_token_name(get_chunk_parent_type(pc)));
   pc->type = token;
} // set_chunk_type_real


void set_chunk_parent_real(chunk_t *pc, c_token_t token, const char *func, int line)
{
   LOG_FUNC_ENTRY();

   if (  pc == nullptr
      || get_chunk_parent_type(pc) == token)
   {
      return;
   }
   LOG_FMT(LSETPAR, "%s(%d): orig_line is %zu, orig_col is %zu, pc->text() ",
           func, line, pc->orig_line, pc->orig_col);

   if (token == CT_NEWLINE)
   {
      LOG_FMT(LSETPAR, "<Newline>\n");
   }
   else
   {
      char copy[1000];
      LOG_FMT(LSETPAR, "'%s'\n", pc->elided_text(copy));
   }
   LOG_FMT(LSETPAR, "   pc->type is %s, pc->parent_type is %s => *type is %s, *parent_type is %s\n",
           get_token_name(pc->type), get_token_name(get_chunk_parent_type(pc)),
           get_token_name(token), get_token_name(get_chunk_parent_type(pc)));
   pc->parent_type = token;
} // set_chunk_parent_real


c_token_t get_chunk_parent_type(chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   if (pc == nullptr)
   {
      return(CT_NONE);
   }
   return(pc->parent_type);
} // get_chunk_parent_type


static chunk_t *chunk_add(const chunk_t *pc_in, chunk_t *ref, const direction_e pos)
{
#ifdef DEBUG
   // test if the pc_in chunk is properly set
   if (pc_in->pp_level == 999)
   {
      fprintf(stderr, "%s(%d): pp_level is not set\n", __func__, __LINE__);
      log_func_stack_inline(LSETFLG);
      log_flush(true);
      exit(EX_SOFTWARE);
   }

   if (pc_in->orig_line == 0)
   {
      fprintf(stderr, "%s(%d): no line number\n", __func__, __LINE__);
      log_func_stack_inline(LSETFLG);
      log_flush(true);
      exit(EX_SOFTWARE);
   }

   if (pc_in->orig_col == 0)
   {
      fprintf(stderr, "%s(%d): no column number\n", __func__, __LINE__);
      log_func_stack_inline(LSETFLG);
      log_flush(true);
      exit(EX_SOFTWARE);
   }
#endif /* DEBUG */

   chunk_t *pc = chunk_dup(pc_in);

   if (pc != nullptr)
   {
      if (ref != nullptr) // ref is a valid chunk
      {
         (pos == direction_e::FORWARD) ? g_cl.AddAfter(pc, ref) : g_cl.AddBefore(pc, ref);
      }
      else // ref == NULL
      {
         (pos == direction_e::FORWARD) ? g_cl.AddHead(pc) : g_cl.AddTail(pc);
      }
      chunk_log(pc, "chunk_add(A):");
   }
   return(pc);
} // chunk_add


chunk_t *chunk_get_next_ssq(chunk_t *cur)
{
   while (  chunk_is_token(cur, CT_TSQUARE)
         || chunk_is_token(cur, CT_SQUARE_OPEN))
   {
      if (chunk_is_token(cur, CT_SQUARE_OPEN))
      {
         cur = chunk_skip_to_match(cur);
      }
      cur = chunk_get_next_nc_nnl(cur);
   }
   return(cur);
}


chunk_t *chunk_get_prev_ssq(chunk_t *cur)
{
   while (  chunk_is_token(cur, CT_TSQUARE)
         || chunk_is_token(cur, CT_SQUARE_CLOSE))
   {
      if (chunk_is_token(cur, CT_SQUARE_CLOSE))
      {
         cur = chunk_skip_to_match_rev(cur);
      }
      cur = chunk_get_prev_nc_nnl(cur);
   }
   return(cur);
}


chunk_t *chunk_get_pp_start(chunk_t *cur)
{
   if (!chunk_is_preproc(cur))
   {
      return(nullptr);
   }

   while (!chunk_is_token(cur, CT_PREPROC))
   {
      cur = chunk_get_prev(cur, scope_e::PREPROC);
   }
   return(cur);
}


//! skip to the final word/type in a :: chain
static chunk_t *chunk_skip_dc_member(chunk_t *start, scope_e scope, direction_e dir)
{
   LOG_FUNC_ENTRY();

   if (start == nullptr)
   {
      return(nullptr);
   }
   const auto step_fcn = (dir == direction_e::FORWARD)
                         ? chunk_get_next_nc_nnl : chunk_get_prev_nc_nnl;

   chunk_t *pc   = start;
   chunk_t *next = chunk_is_token(pc, CT_DC_MEMBER) ? pc : step_fcn(pc, scope);

   while (chunk_is_token(next, CT_DC_MEMBER))
   {
      pc = step_fcn(next, scope);

      if (pc == nullptr)
      {
         return(nullptr);
      }
      next = step_fcn(pc, scope);
   }
   return(pc);
}


chunk_t *chunk_skip_dc_member(chunk_t *start, scope_e scope)
{
   return(chunk_skip_dc_member(start, scope, direction_e::FORWARD));
}


chunk_t *chunk_skip_dc_member_rev(chunk_t *start, scope_e scope)
{
   return(chunk_skip_dc_member(start, scope, direction_e::BACKWARD));
}


// set parent member
void chunk_set_parent(chunk_t *pc, chunk_t *parent)
{
   if (pc == nullptr)
   {
      return;
   }

   if (parent == nullptr)
   {
      return;
   }

   if (pc == parent)
   {
      return;
   }
   pc->parent = parent;
}


c_token_t get_type_of_the_parent(chunk_t *pc)
{
   if (pc == nullptr)
   {
      return(CT_UNKNOWN);
   }

   if (pc->parent == nullptr)
   {
      return(CT_PARENT_NOT_SET);
   }
   return(pc->parent->type);
}


bool chunk_is_attribute_or_declspec(chunk_t *pc)
{
   return(  language_is_set(LANG_CPP)
         && (  chunk_is_token(pc, CT_ATTRIBUTE)
            || chunk_is_token(pc, CT_DECLSPEC)));
}


bool chunk_is_class_enum_struct_union(chunk_t *pc)
{
   return(  chunk_is_class_or_struct(pc)
         || chunk_is_enum(pc)
         || chunk_is_token(pc, CT_UNION));
}


bool chunk_is_class_or_struct(chunk_t *pc)
{
   return(  chunk_is_token(pc, CT_CLASS)
         || chunk_is_token(pc, CT_STRUCT));
}


bool chunk_is_class_struct_union(chunk_t *pc)
{
   return(  chunk_is_class_or_struct(pc)
         || chunk_is_token(pc, CT_UNION));
}


bool chunk_is_enum(chunk_t *pc)
{
   return(  chunk_is_token(pc, CT_ENUM)
         || chunk_is_token(pc, CT_ENUM_CLASS));
}


int chunk_compare_position(const chunk_t *A_token, const chunk_t *B_token)
{
   if (A_token == nullptr)
   {
      assert(A_token);
   }

   if (B_token == nullptr)
   {
      assert(B_token);
   }

   if (A_token->orig_line < B_token->orig_line)
   {
      return(-1);
   }
   else if (A_token->orig_line == B_token->orig_line)
   {
      if (A_token->orig_col < B_token->orig_col)
      {
         return(-1);
      }
      else if (A_token->orig_col == B_token->orig_col)
      {
         return(0);
      }
   }
   return(1);
}
