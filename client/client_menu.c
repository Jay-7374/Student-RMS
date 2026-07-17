/**
 * @file    client_menu.c
 * @brief   Interactive CLI menu and client entry point.
 *
 * Responsibilities:
 *   - Connect to the server.
 *   - Display the main menu with ANSI colors.
 *   - Collect user input for each operation.
 *   - Build a Request struct and send it via client.c.
 *   - Receive and render the server's Response.
 *   - Repeat until the user chooses Exit.
 *
 * Usage:
 *   client.exe [host] [port]       (defaults: 127.0.0.1  8080)
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"
#include "../common/protocol.h"
#include "../common/student.h"
#include "../common/validation.h"

/* =========================================================================
 * ANSI color codes  (disabled automatically on non-ANSI terminals)
 * ========================================================================= */

#define COL_RESET   "\033[0m"
#define COL_BOLD    "\033[1m"
#define COL_CYAN    "\033[36m"
#define COL_GREEN   "\033[32m"
#define COL_YELLOW  "\033[33m"
#define COL_RED     "\033[31m"
#define COL_MAGENTA "\033[35m"
#define COL_BLUE    "\033[34m"
#define COL_WHITE   "\033[37m"

/* =========================================================================
 * Display helpers
 * ========================================================================= */

static void clear_screen(void)
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

static void print_separator(void)
{
    printf(COL_CYAN "══════════════════════════════════════════════\n" COL_RESET);
}

static void print_banner(void)
{
    clear_screen();
    printf(COL_CYAN COL_BOLD);
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     Student Database Management System       ║\n");
    printf("║           Client  —  Version 2.0.0           ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf(COL_RESET "\n");
}

static void print_menu(void)
{
    print_separator();
    printf(COL_BOLD COL_WHITE " MAIN MENU\n" COL_RESET);
    print_separator();
    printf(COL_GREEN "  1." COL_RESET " Add Student\n");
    printf(COL_GREEN "  2." COL_RESET " View All Students\n");
    printf(COL_GREEN "  3." COL_RESET " Search Student\n");
    printf(COL_GREEN "  4." COL_RESET " Update Student\n");
    printf(COL_GREEN "  5." COL_RESET " Delete Student\n");
    printf(COL_GREEN "  6." COL_RESET " Reports\n");
    printf(COL_GREEN "  7." COL_RESET " Ping Server\n");
    printf(COL_RED   "  8." COL_RESET " Exit\n");
    print_separator();
    printf(COL_YELLOW "Choose option: " COL_RESET);
}

static void press_enter(void)
{
    printf(COL_YELLOW "\nPress Enter to continue..." COL_RESET);
    fflush(stdout);
    getchar();
}

/* =========================================================================
 * Student display helpers
 * ========================================================================= */

static void print_table_header(void)
{
    printf(COL_CYAN COL_BOLD);
    printf("%-4s %-8s %-22s %-18s %-5s %-6s\n",
           "#", "ID", "Name", "Department", "Year", "GPA");
    printf("──── ──────── ────────────────────── "
           "────────────────── ───── ──────\n");
    printf(COL_RESET);
}

static void print_student_row(const Student *s, int idx)
{
    printf("%-4d %-8d %-22s %-18s %-5d %.2f\n",
           idx, s->student_id, s->name, s->department,
           s->year_of_study, s->gpa);
}

static void print_student_detail(const Student *s)
{
    char date_buf[32] = {0};
    struct tm *tm_info = localtime(&s->enrollment_date);
    if (tm_info) strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_info);

    print_separator();
    printf(COL_BOLD COL_WHITE " STUDENT RECORD DETAIL\n" COL_RESET);
    print_separator();
    printf(COL_CYAN "  ID           : " COL_RESET "%d\n",   s->student_id);
    printf(COL_CYAN "  Name         : " COL_RESET "%s\n",   s->name);
    printf(COL_CYAN "  Roll Number  : " COL_RESET "%d\n",   s->roll_number);
    printf(COL_CYAN "  Department   : " COL_RESET "%s\n",   s->department);
    printf(COL_CYAN "  Year         : " COL_RESET "%d\n",   s->year_of_study);
    printf(COL_CYAN "  Age          : " COL_RESET "%d\n",   s->age);
    printf(COL_CYAN "  GPA          : " COL_RESET "%.2f\n", s->gpa);
    printf(COL_CYAN "  Email        : " COL_RESET "%s\n",   s->email);
    printf(COL_CYAN "  Phone        : " COL_RESET "%s\n",   s->phone);
    printf(COL_CYAN "  Address      : " COL_RESET "%s\n",   s->address);
    printf(COL_CYAN "  Enrolled     : " COL_RESET "%s\n",   date_buf);
    print_separator();
}

static void display_response_list(const Response *resp)
{
    int i;
    if (resp->count == 0) {
        printf(COL_YELLOW "  No records found.\n" COL_RESET);
        return;
    }
    print_table_header();
    for (i = 0; i < resp->count; i++) {
        print_student_row(&resp->students[i], i + 1);
    }
    printf("\n  Total: %d record(s)\n", resp->count);
}

static void check_response_status(const Response *resp)
{
    if (resp->status == RESULT_OK) {
        printf(COL_GREEN "  ✓ %s\n" COL_RESET, resp->message);
    } else {
        printf(COL_RED "  ✗ Error: %s\n" COL_RESET, resp->message);
    }
}

/* =========================================================================
 * Input helpers (wraps validation.h)
 * ========================================================================= */

static void read_string_field(const char *label, char *buf, size_t size,
                              int allow_empty)
{
    val_read_string(label, buf, size, allow_empty);
}

static void collect_student(Student *s)
{
    memset(s, 0, sizeof(Student));
    s->enrollment_date = time(NULL);
    s->is_active       = 1;

    print_separator();
    printf(COL_BOLD " Enter Student Details\n" COL_RESET);
    print_separator();

    val_read_int("  Student ID   : ", 1, 99999999, &s->student_id);
    read_string_field("  Full Name    : ", s->name, sizeof(s->name), 0);
    val_trim(s->name);
    val_to_title_case(s->name);

    val_read_int("  Roll Number  : ", 1, 99999, &s->roll_number);
    read_string_field("  Department   : ", s->department, sizeof(s->department), 0);
    val_trim(s->department);
    val_to_title_case(s->department);

    val_read_int("  Year (1-6)   : ", YEAR_MIN, YEAR_MAX, &s->year_of_study);
    val_read_int("  Age          : ", AGE_MIN,  AGE_MAX,  &s->age);
    val_read_float("  GPA (0-10)   : ", GPA_MIN, GPA_MAX, &s->gpa);

    /* Email — loop until valid */
    do {
        read_string_field("  Email        : ", s->email, sizeof(s->email), 0);
    } while (!val_is_valid_email(s->email) &&
             (printf(COL_RED "  Invalid email format. Try again.\n" COL_RESET), 1));

    /* Phone — loop until valid */
    do {
        read_string_field("  Phone        : ", s->phone, sizeof(s->phone), 0);
    } while (!val_is_valid_phone(s->phone) &&
             (printf(COL_RED "  Invalid phone format. Try again.\n" COL_RESET), 1));

    read_string_field("  Address      : ", s->address, sizeof(s->address), 1);
}

/* =========================================================================
 * Menu action handlers
 * ========================================================================= */

static void action_add(ClientConn *conn)
{
    Request  req  = {0};
    Response resp = {0};

    print_banner();
    printf(COL_BOLD COL_CYAN " [ ADD STUDENT ]\n" COL_RESET);

    req.operation = OP_ADD;
    collect_student(&req.student);

    printf(COL_YELLOW "\n  Sending to server...\n" COL_RESET);
    if (client_exchange(conn, &req, &resp) != 0) {
        printf(COL_RED "  Network error.\n" COL_RESET);
    } else {
        check_response_status(&resp);
    }
    press_enter();
}

static void action_view_all(ClientConn *conn)
{
    Request  req  = {0};
    Response resp = {0};
    int      sort_choice;

    print_banner();
    printf(COL_BOLD COL_CYAN " [ VIEW ALL STUDENTS ]\n" COL_RESET);
    print_separator();
    printf("  Sort by:\n");
    printf(COL_GREEN "  1." COL_RESET " Name   "
           COL_GREEN "2." COL_RESET " GPA (desc)   "
           COL_GREEN "3." COL_RESET " Roll   "
           COL_GREEN "4." COL_RESET " ID   "
           COL_GREEN "0." COL_RESET " None\n");
    val_read_int("  Choice: ", 0, 4, &sort_choice);

    req.operation = OP_VIEW_ALL;
    req.sort_key  = sort_choice;

    if (client_exchange(conn, &req, &resp) != 0) {
        printf(COL_RED "  Network error.\n" COL_RESET);
    } else {
        printf("\n");
        display_response_list(&resp);
    }
    press_enter();
}

static void action_search(ClientConn *conn)
{
    Request  req  = {0};
    Response resp = {0};
    int      search_choice;

    print_banner();
    printf(COL_BOLD COL_CYAN " [ SEARCH STUDENT ]\n" COL_RESET);
    print_separator();
    printf(COL_GREEN "  1." COL_RESET " Search by ID\n");
    printf(COL_GREEN "  2." COL_RESET " Search by Name\n");
    printf(COL_GREEN "  3." COL_RESET " Search by Department\n");
    val_read_int("  Choice: ", 1, 3, &search_choice);

    req.operation    = OP_SEARCH;
    req.search_type  = search_choice;

    switch (search_choice) {
        case SEARCH_BY_ID:
            val_read_int("  Student ID: ", 1, 99999999, &req.student_id);
            break;
        case SEARCH_BY_NAME:
            read_string_field("  Name keyword: ", req.search_keyword,
                              sizeof(req.search_keyword), 0);
            break;
        case SEARCH_BY_DEPT:
            read_string_field("  Department: ", req.search_keyword,
                              sizeof(req.search_keyword), 0);
            break;
    }

    if (client_exchange(conn, &req, &resp) != 0) {
        printf(COL_RED "  Network error.\n" COL_RESET);
    } else if (resp.status == RESULT_OK) {
        printf("\n");
        if (resp.count == 1) {
            print_student_detail(&resp.students[0]);
        } else {
            display_response_list(&resp);
        }
    } else {
        check_response_status(&resp);
    }
    press_enter();
}

static void action_update(ClientConn *conn)
{
    Request  req  = {0};
    Response resp = {0};
    int      target_id;
    Student *cur;          /* pointer to the fetched record */
    char     buf[256];     /* temporary input buffer        */
    int      ival;
    float    fval;

    print_banner();
    printf(COL_BOLD COL_CYAN " [ UPDATE STUDENT ]\n" COL_RESET);

    /* ── Step 1: fetch existing record ─────────────────────────── */
    val_read_int("  Enter Student ID to update: ", 1, 99999999, &target_id);

    req.operation   = OP_SEARCH;
    req.search_type = SEARCH_BY_ID;
    req.student_id  = target_id;

    if (client_exchange(conn, &req, &resp) != 0 || resp.count == 0) {
        printf(COL_RED "  Student ID %d not found.\n" COL_RESET, target_id);
        press_enter();
        return;
    }

    cur = &resp.students[0];
    print_student_detail(cur);

    printf(COL_YELLOW
           "  Enter new values below.\n"
           "  Press Enter on any field to keep the current value.\n\n"
           COL_RESET);
    print_separator();
    printf(COL_BOLD " Update Fields\n" COL_RESET);
    print_separator();

    /* ── Step 2: collect updates — empty = keep current ─────────── */

    /* Start with a copy of the current record */
    memset(&req, 0, sizeof(req));
    req.operation = OP_UPDATE;
    req.student   = *cur;   /* pre-fill ALL fields from the live record */

    /* --- Full Name --- */
    printf("  Full Name    [%s]: ", cur->name);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        val_trim(buf);
        val_to_title_case(buf);
        strncpy(req.student.name, buf, sizeof(req.student.name) - 1);
    }

    /* --- Roll Number --- */
    printf("  Roll Number  [%d]: ", cur->roll_number);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (sscanf(buf, "%d", &ival) == 1 && ival >= 1 && ival <= 99999)
            req.student.roll_number = ival;
        else
            printf(COL_YELLOW "  Invalid — keeping %d.\n" COL_RESET,
                   cur->roll_number);
    }

    /* --- Department --- */
    printf("  Department   [%s]: ", cur->department);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        val_trim(buf);
        val_to_title_case(buf);
        strncpy(req.student.department, buf,
                sizeof(req.student.department) - 1);
    }

    /* --- Year --- */
    printf("  Year (1-6)   [%d]: ", cur->year_of_study);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (sscanf(buf, "%d", &ival) == 1 &&
            ival >= YEAR_MIN && ival <= YEAR_MAX)
            req.student.year_of_study = ival;
        else
            printf(COL_YELLOW "  Invalid — keeping %d.\n" COL_RESET,
                   cur->year_of_study);
    }

    /* --- Age --- */
    printf("  Age          [%d]: ", cur->age);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (sscanf(buf, "%d", &ival) == 1 &&
            ival >= AGE_MIN && ival <= AGE_MAX)
            req.student.age = ival;
        else
            printf(COL_YELLOW "  Invalid — keeping %d.\n" COL_RESET,
                   cur->age);
    }

    /* --- GPA --- */
    printf("  GPA (0-10)   [%.2f]: ", cur->gpa);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (sscanf(buf, "%f", &fval) == 1 &&
            fval >= GPA_MIN && fval <= GPA_MAX)
            req.student.gpa = fval;
        else
            printf(COL_YELLOW "  Invalid — keeping %.2f.\n" COL_RESET,
                   cur->gpa);
    }

    /* --- Email --- */
    printf("  Email        [%s]: ", cur->email);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (val_is_valid_email(buf))
            strncpy(req.student.email, buf,
                    sizeof(req.student.email) - 1);
        else
            printf(COL_YELLOW "  Invalid email — keeping current.\n"
                   COL_RESET);
    }

    /* --- Phone --- */
    printf("  Phone        [%s]: ", cur->phone);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (val_is_valid_phone(buf))
            strncpy(req.student.phone, buf,
                    sizeof(req.student.phone) - 1);
        else
            printf(COL_YELLOW "  Invalid phone — keeping current.\n"
                   COL_RESET);
    }

    /* --- Address --- */
    printf("  Address      [%s]: ", cur->address);
    fflush(stdout);
    val_read_line(buf, sizeof(buf));
    if (buf[0] != '\0')
        strncpy(req.student.address, buf,
                sizeof(req.student.address) - 1);

    /* Preserve the original ID and active flag */
    req.student.student_id = target_id;
    req.student.is_active  = 1;

    /* ── Step 3: send to server ──────────────────────────────────── */
    printf(COL_YELLOW "\n  Saving changes...\n" COL_RESET);
    if (client_exchange(conn, &req, &resp) != 0) {
        printf(COL_RED "  Network error.\n" COL_RESET);
    } else {
        check_response_status(&resp);
    }
    press_enter();
}


static void action_delete(ClientConn *conn)
{
    Request  req  = {0};
    Response resp = {0};
    int      target_id;
    char     confirm[8];

    print_banner();
    printf(COL_BOLD COL_CYAN " [ DELETE STUDENT ]\n" COL_RESET);

    val_read_int("  Enter Student ID to delete: ", 1, 99999999, &target_id);

    printf(COL_RED "  Are you sure you want to delete ID %d? (yes/no): "
           COL_RESET, target_id);
    fflush(stdout);
    val_read_line(confirm, sizeof(confirm));

    if (strcmp(confirm, "yes") != 0) {
        printf(COL_YELLOW "  Delete cancelled.\n" COL_RESET);
        press_enter();
        return;
    }

    req.operation  = OP_DELETE;
    req.student_id = target_id;

    if (client_exchange(conn, &req, &resp) != 0) {
        printf(COL_RED "  Network error.\n" COL_RESET);
    } else {
        check_response_status(&resp);
    }
    press_enter();
}

static void action_report(ClientConn *conn)
{
    Request  req  = {0};
    Response resp = {0};

    print_banner();
    printf(COL_BOLD COL_CYAN " [ REPORTS ]\n" COL_RESET);
    print_separator();

    req.operation = OP_REPORT;

    if (client_exchange(conn, &req, &resp) != 0) {
        printf(COL_RED "  Network error.\n" COL_RESET);
        press_enter();
        return;
    }

    if (resp.status != RESULT_OK) {
        check_response_status(&resp);
        press_enter();
        return;
    }

    printf(COL_CYAN "  Total Active Students : " COL_RESET "%d\n",
           resp.total_active);
    printf(COL_CYAN "  Average GPA           : " COL_RESET "%.2f\n",
           resp.avg_gpa);
    printf(COL_CYAN "  Highest GPA           : " COL_RESET "%.2f\n",
           resp.top_gpa);
    printf(COL_CYAN "  Top Performer         : " COL_RESET "%s\n",
           resp.top_student_name[0] ? resp.top_student_name : "N/A");
    print_separator();
    press_enter();
}

static void action_ping(ClientConn *conn)
{
    Request  req  = {0};
    Response resp = {0};

    req.operation = OP_PING;
    if (client_exchange(conn, &req, &resp) != 0) {
        printf(COL_RED "  Server unreachable.\n" COL_RESET);
    } else {
        printf(COL_GREEN "  Server is online: %s\n" COL_RESET, resp.message);
    }
    press_enter();
}

/* =========================================================================
 * main()
 * ========================================================================= */

int main(int argc, char *argv[])
{
    const char *host = DEFAULT_HOST;
    int         port = DEFAULT_PORT;
    ClientConn  conn;
    int         choice;

#ifdef _WIN32
    /* Switch the Windows console to UTF-8 so box-drawing characters
     * (╔ ═ ║ ╚ etc.) render correctly without needing 'chcp 65001'. */
    SetConsoleOutputCP(65001);
#endif

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    print_banner();
    printf(COL_YELLOW "  Connecting to %s:%d ...\n\n" COL_RESET, host, port);

    if (client_connect(&conn, host, port) != 0) {
        printf(COL_RED
               "  Could not connect to server.\n"
               "  Make sure server.exe is running on %s:%d\n"
               COL_RESET, host, port);
        return EXIT_FAILURE;
    }

    /* ── Main menu loop ────────────────────────────────────────── */
    while (client_is_connected(&conn)) {
        print_banner();
        print_menu();

        if (scanf("%d", &choice) != 1) {
            /* Clear bad input */
            while (getchar() != '\n') {}
            continue;
        }
        getchar(); /* consume trailing newline */

        switch (choice) {
            case 1: action_add      (&conn); break;
            case 2: action_view_all (&conn); break;
            case 3: action_search   (&conn); break;
            case 4: action_update   (&conn); break;
            case 5: action_delete   (&conn); break;
            case 6: action_report   (&conn); break;
            case 7: action_ping     (&conn); break;
            case 8:
                printf(COL_YELLOW "  Disconnecting from server...\n" COL_RESET);
                client_disconnect(&conn);
                break;
            default:
                printf(COL_RED "  Invalid option. Please choose 1-8.\n" COL_RESET);
                press_enter();
                break;
        }
    }

    printf(COL_CYAN "\n  Thank you for using Student Database System.\n"
           COL_RESET);
    return EXIT_SUCCESS;
}
