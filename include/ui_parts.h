#ifndef UI_PARTS_H
#define UI_PARTS_H

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <vector>
#include <iterator>
#include <mutex>
#include <string>
#include <yder.h>
#include <db_handle.h>
#include <ctype.h>
#include <cstring>

void new_part_window( bool* show, struct dbinfo_t** info );
void edit_part_window( bool* show, struct part_t* part_in, struct dbinfo_t** info );

#endif /* UI_PARTS_H */
