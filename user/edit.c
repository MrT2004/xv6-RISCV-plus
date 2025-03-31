#include "kernel/types.h"
#include "user.h"
#include "kernel/fcntl.h"

#define NULL 0
#define MAXFILE 71680

typedef struct listNode
{
    char *line;
    struct listNode *next;
} listNode;

typedef struct
{
    listNode *head;
    int total_bytes;
    int line_count;
    int fd;
    char *name;
} file;

typedef struct
{
    int begin;
    int end;
} range;
int findSubstring(char *haystack, char *needle)
{
    int i = 0;
    while (haystack[i] != '\0')
    {
        int j = 0;
        while (needle[j] != '\0' && haystack[i + j] == needle[j])
        {
            j++;
        }
        if (needle[j] == '\0')
        {
            return 1;
        }
        i++;
    }
    return 0;
}
int integer(const char *s)
{
    int n;
    int sign = 1;
    // Handle negative sign
    if (*s == '-')
    {
        sign = -1;
        s++;
    }

    n = 0;
    while ('0' <= *s && *s <= '9')
        n = n * 10 + *s++ - '0';
    return sign * n;
}
void *reallocateMemory(void *ptr, uint old_size, uint new_size)
{
    if (new_size <= old_size)
        return ptr;

    void *new_ptr = malloc(new_size);
    if (new_ptr == NULL)
        return NULL;

    if (ptr != NULL)
    {
        memmove(new_ptr, ptr, old_size);
        free(ptr);
    }

    return new_ptr;
}
int createNode(listNode **node, char *line, int i)
{
    *node = (listNode *)malloc(sizeof(listNode));
    if (*node == NULL)
    {
        printf("Error: Memory allocation failed for new node\n");
        return NULL;
    }
    // Newly created lines should always end in a newline.
    //They might already have one though because of copy
    int add_newline = 0;
    if (line[i - 1] != '\n')
    {
        add_newline = 1;
    }
    if (add_newline)
    {
        (*node)->line = (char *)malloc(sizeof(char) * (i + 2)); // Allocate space for `i` chars + newline + null terminator
    }
    else
    {
        (*node)->line = (char *)malloc(sizeof(char) * (i + 1)); // Allocate space for `i` chars + null terminator
    }
    if ((*node)->line == NULL)
    {
        printf("Error: Memory allocation failed for line\n");
        exit(0); // Exit if memory allocation fails
    }

    for (int j = 0; j < i; j++)
    {
        (*node)->line[j] = line[j];
    }
    if (add_newline)
    {
        (*node)->line[i] = '\n';     // Add newline character
        (*node)->line[i + 1] = '\0'; // Null-terminate the string
    }
    else
    {
        (*node)->line[i] = '\0'; // Null-terminate the string
    }
    return 1;
}
// creates a node after current or creates a head if list is empty
// returns number of bytes in the line
int createNodeBuf(char *buffer, int i, listNode **head, listNode **current)
{
    if (i <= 0)
    {
        printf("Invalid line length: %d\n", i);
        return 0;
    }

    // Allocate memory for the first node if it doesn't exist
    if (*head == 0)
    {
        *head = (listNode *)malloc(sizeof(listNode));
        if (*head == NULL)
        {
            printf("Error: Memory allocation failed for node creation\n");
            exit(0); // Exit if memory allocation fails
        }
        *current = *head;
    }
    else
    {
        (*current)->next = (listNode *)malloc(sizeof(listNode));
        if ((*current)->next == NULL)
        {
            printf("Error: Memory allocation failed for next node\n");
            exit(0); // Exit if memory allocation fails
        }
        *current = (*current)->next;
    }
    (*current)->line = (char *)malloc(sizeof(char) * (i + 1)); // Allocate space for `i` chars + null terminator
    if ((*current)->line == NULL)
    {
        printf("Error: Memory allocation failed for line\n");
        exit(0); // Exit if memory allocation fails
    }

    for (int j = 0; j < i; j++)
    {
        (*current)->line[j] = buffer[j];
    }
    (*current)->line[i] = '\0'; // Null-terminate the string
    (*current)->next = NULL; // Set next pointer to NULL
    return i;
}
file readFile(int fd)
{
    listNode *head = 0;
    listNode *current = 0;
    char buffer[512];
    int read_bytes;
    int total_bytes = 0;
    char extra[512];
    int extra_bytes = 0;
    int line_count = 0;
    char *line = NULL;
    int line_len = 0;

    while ((read_bytes = read(fd, buffer + extra_bytes, sizeof(buffer) - extra_bytes)) || extra_bytes > 0)
    {
        read_bytes += extra_bytes; // Account for leftover bytes
        if (read_bytes + total_bytes > MAXFILE)
        {
            printf("File too large\n");
            exit(0);
        }

        // Move any leftover data from the previous read into the buffer
        memmove(buffer, extra, extra_bytes);
        int split_index = -1; // Where to split the buffer
        for (int i = 0; i < read_bytes; i++)
        { // Search for '\n' from the beginning
            if (buffer[i] == '\n')
            {
                split_index = i + 1; // Include the delimiter
                break;
            }
        }

        if (split_index != -1 || read_bytes == 0)
        {
            if (line_len > 0)
            {
                line = reallocateMemory(line, line_len, sizeof(char) * (split_index + line_len));
                printf("line_len: %d\n", line_len);
                printf("old line: %s\n", line);
                memmove(line + line_len, buffer, split_index);
                printf("new line: %s\n", line);
                line_len += read_bytes;
                total_bytes += createNodeBuf(line, line_len, &head, &current);
                line_count++;
                free(line);
                line_len = 0;
            }
            else
            {
                total_bytes += createNodeBuf(buffer, split_index, &head, &current);
                line_count++;
            }

            // Store leftover bytes in `extra`
            extra_bytes = read_bytes - split_index;
            memmove(extra, buffer + split_index, extra_bytes);
        }
        else
        {
            // No newline found, store everything
            line = reallocateMemory(line, line_len, sizeof(char) * (read_bytes + line_len));
            memmove(line + line_len, buffer, read_bytes);
            line_len += read_bytes;
            extra_bytes = 0;
        }
    }
    file result;
    result.head = head;
    result.total_bytes = total_bytes;
    result.line_count = line_count;
    result.fd = fd;
    return result;
}
// Parse a range string in the format "begin:end"
range parseRange(char *arg, int line_count)
{
    range r;
    r.begin = 0;
    r.end = 99999999;
    int seen_colon = 0;
    char *begin_ptr = arg;
    char *end_ptr = NULL;

    while (*arg != '\0')
    {
        if (*arg == ':')
        {
            seen_colon = 1;
            *arg = '\0';       // Null-terminate the beginning part
            end_ptr = arg + 1; // Set end_ptr to the character after ':'
        }
        arg++;
    }
    if (seen_colon)
    {
        if (*begin_ptr != NULL)
        {
            r.begin = integer(begin_ptr);
        }
        if (*end_ptr != NULL)
        {
            r.end = integer(end_ptr);
        }
    }
    else
    {
        r.begin = integer(begin_ptr);
        r.end = r.begin;
    }
    if (r.begin < 0)
    {
        r.begin = line_count + r.begin + 1;
    }
    if (r.end < 0)
    {
        r.end = line_count + r.end + 1;
    }
    return r;
}
void printList(listNode *head)
{
    listNode *current = head;
    int line_number = 1;
    while (current != 0)
    {
        printf("%d: %s", line_number, current->line);
        current = current->next;
        line_number++;
    }
}
int end(file *f, char *line)
{
    listNode *current = f->head;
    if (current == NULL)
    {
        // List is empty, create the first node
        if (createNode(&f->head, line, strlen(line)))
            f->line_count++;
        printf("Line added\n");
        return 0;
    }
    while (current->next != 0)
    {
        listNode *next = current->next;
        current = next;
    }
    // Adding a new line implicitly means adding \n to old last line if its not there
    if (current->line[strlen(current->line) - 1] != '\n')
    {
        int i = strlen(current->line);
        current->line = (char *)reallocateMemory(current->line, i, sizeof(char) * (i + 2));
        if (current->line == NULL)
        {
            printf("Error: Memory allocation failed for new node\n");
            return 1;
        }
        current->line[i] = '\n';
        current->line[i + 1] = '\0';
    }
    listNode *new_node;
    if (createNode(&new_node, line, strlen(line)))
        f->line_count++;
    current->next = new_node;
    printf("Line added\n");
    return 0;
}
int add(file *f, char *line, int line_number)
{
    listNode *current = f->head;
    int i = strlen(line);
    if (i > 80)
    {
        printf("Line too long\n");
        return 1;
    }
    if (f->total_bytes + i > MAXFILE)
    {
        printf("File too large\n");
        return 1;
    }
    if (line_number == 1)
    {
        listNode *new_head;
        if (createNode(&new_head, line, i))
            f->line_count++;

        new_head->line[i] = '\0';
        new_head->next = current;
        f->head = new_head;
        printf("Line added\n");
        return 0;
    }
    if (line_number > f->line_count)
    {
        printf("Line number too large\n");
        return 1;
    }
    for (int j = 1; j < line_number - 1; j++)
    {
        current = current->next;
    }
    listNode *new_node;
    if (createNode(&new_node, line, i))
        f->line_count++;
    new_node->next = current->next;
    current->next = new_node;
    printf("Line added\n");
    return 0;
}
int edit(file *f, char *line, int line_number)
{
    listNode *current = f->head;
    int i = strlen(line);
    if (i > 80)
    {
        printf("Line too long\n");
        return 1;
    }
    for (int j = 1; j < line_number; j++)
    {
        if (current->next == 0)
        {
            printf("Line number too large\n");
            return 1;
        }
        current = current->next;
    }
    if (f->total_bytes + (strlen(current->line) - i) > MAXFILE)
    {
        printf("File too large\n");
        return 1;
    }
    if (current->line != 0)
    {
        free(current->line);
    }
    current->line = (char *)malloc(sizeof(char) * (i + 2));
    if (current->line == NULL)
    {
        printf("Error: Memory allocation failed for new node\n");
        return 1;
    }
    for (int j = 0; j < i; j++)
    {
        current->line[j] = line[j];
    }
    current->line[i] = '\n';
    current->line[i + 1] = '\0';
    printf("Line edited\n");
    return 0;
}
void list(file *f, range r)
{
    listNode *current = f->head;
    int line_number = 1;

    while (current != 0 && line_number <= r.end)
    {
        if (line_number >= r.begin)
        {
            int line_len = strlen(current->line);
            int offset = 0;

            printf("%d: ", line_number);
            while (1)
            {
                if (offset + 80 >= line_len)
                {
                    printf("%s", current->line + offset);
                    break;
                    ;
                }
                int max_chars = (offset + 80 > line_len) ? line_len - offset : 80;
                int break_point = offset + max_chars;

                // Find the last space within the 80-character limit for word wrapping
                while (break_point > offset && current->line[break_point] != ' ')
                {
                    break_point--;
                }

                // If no space found, break at 80 characters
                if (break_point == offset)
                {
                    break_point = offset + max_chars;
                }

                write(1, current->line + offset, break_point - offset);
                write(1, "\n", 1);

                // Move offset forward, skipping the space
                offset = (current->line[break_point] == ' ') ? break_point + 1 : break_point;
            }
        }

        current = current->next;
        line_number++;
    }
}

int drop(file *f, range r)
{
    listNode *current = f->head;
    listNode *prev = 0;
    int line_number = 1;
    while (current != 0 && line_number <= r.end)
    {
        if (line_number >= r.begin)
        {
            if (prev == 0)
            {
                f->head = current->next;
            }
            else
            {
                prev->next = current->next;
            }
            free(current->line);
            free(current);
            printf("Line %d dropped\n", line_number);
        }
        else
        {
            prev = current;
        }
        current = current->next;
        line_number++;
    }
    if (r.begin >= line_number)
    {
        printf("Line number too large\n");
        return 1;
    }
    return 0;
}

void save(file *f)
{

    listNode *current = f->head;

    // Close the file descriptor
    close(f->fd);

    // Remove the old file
    unlink(f->name);

    // Recreate the file
    f->fd = open(f->name, O_CREATE | O_RDWR);
    if (f->fd < 0)
    {
        printf("Error: Could not open file %s for writing.\n", f->name);
        return;
    }

    // Write the new content to the file
    while (current != NULL)
    {
        write(f->fd, current->line, strlen(current->line));
        current = current->next;
    }
    close(f->fd);
    printf("File saved successfully.\n");
}
void find(file *f, char *substr)
{
    listNode *current = f->head;
    int line_number = 1;
    int found = 0;
    printf("%s is found on line(s):", substr);
    while (current != 0)
    {
        if (findSubstring(current->line, substr) != NULL)
        {
            if (found)
            {
                printf(",");
            }
            printf(" %d", line_number);
            found = 1;
        }
        current = current->next;
        line_number++;
    }
    if (!found)
    {
        printf(" none");
    }
    printf("\n");
}
void help(char *command)
{
    if (command == NULL)
    {
        printf("Available commands:\n");
        printf("ADD< <line_number> <line> - Add a line at the specified line number\n");
        printf("DROP <begin:end> - Drop lines in the specified range\n");
        printf("EDIT <line_number> <line> - Edit a line at the specified line number\n");
        printf("END <line> - Add a line at the end of the file\n");
        printf("FIND <string> - Find a string in the file\n");
        printf("HELP ?<command> - Display help for a specific command\n");
        printf("LIST <begin:end> - List lines in the specified range\n");
        printf("COPY <begin:end> <line_number> - Copy lines in the specified range before the destination line\n");
        printf("OPEN <filename> - Open a new file\n");
        printf("PRINT - Print the entire file\n");
        printf("QUIT - Quit the editor\n");
        printf("SAVE ?<filename> - Save the file to filename or this file if none specified.\n");
    }
    else if (strcmp(command, "ADD<") == 0)
    {
        printf("ADD< <line_number> <line> - Add a line at the specified line number\n");
    }
    else if (strcmp(command, "DROP") == 0)
    {
        printf("DROP <begin:end> - Drop lines in the specified range\n");
    }
    else if (strcmp(command, "EDIT") == 0)
    {
        printf("EDIT <line_number> <line> - Edit a line at the specified line number\n");
    }
    else if (strcmp(command, "END") == 0)
    {
        printf("END <line> - Add a line at the end of the file\n");
    }
    else if (strcmp(command, "FIND") == 0)
    {
        printf("FIND <string> - Find a string in the file\n");
    }
    else if (strcmp(command, "LIST") == 0)
    {
        printf("LIST <begin:end> - List lines in the specified range\n");
    }
    else if (strcmp(command, "OPEN") == 0)
    {
        printf("OPEN <filename> - Open a new file\n");
    }
    else if (strcmp(command, "PRINT") == 0)
    {
        printf("PRINT - Print the entire file\n");
    }
    else if (strcmp(command, "COPY") == 0)
    {
        printf("COPY <begin:end> <line_number> - Copy lines in the specified range before the destination line\n");
    }
    else if (strcmp(command, "QUIT") == 0)
    {
        printf("QUIT - Quit the editor\n");
    }
    else if (strcmp(command, "SAVE") == 0)
    {
        printf("SAVE ?<filename> - Save the file to filename or this file if none specified.\n");
    }
    else
    {
        printf("HELP ?<command> - Display help for a specific command\n");
    }
}
// Copy the range of lines AFTER the destination line
int copy(file *f, range *rng, int dest_line)
{
    if (dest_line < 0 || dest_line > f->line_count)
    {
        printf("Invalid destination line number\n");
        return 1;
    }
    if (rng->begin < 1)
    {
        rng->begin = 1;
    }
    if (rng->end > f->line_count)
    {
        rng->end = f->line_count;
    }
    if (rng->begin > rng->end)
    {
        printf("Invalid range\n");
        return 1;
    }

    listNode *current = f->head;
    listNode *dest = NULL;
    listNode *new_head = NULL;
    listNode *last = NULL;
    int line_number = 1;

    // Locate the destination node
    while (current != NULL && line_number <= dest_line)
    {
        if (line_number == dest_line)
        {
            dest = current;
        }
        current = current->next;
        line_number++;
    }

    // Start copying nodes within the specified range
    current = f->head;
    line_number = 1;

    while (current != NULL && line_number <= rng->end)
    {
        if (line_number >= rng->begin && line_number <= rng->end)
        {
            listNode *new_node = NULL;
            if (!createNode(&new_node, current->line, strlen(current->line)))
            {
                return 1; // Memory allocation failure
            }
            if (new_head == NULL)
            {
                new_head = new_node;
            }
            else
            {
                last->next = new_node;
            }
            last = new_node;
        }
        current = current->next;
        line_number++;
    }

    // Insert copied nodes after the destination
    if (dest != NULL)
    {
        last->next = dest->next;
        dest->next = new_head;
    }
    else
    {
        // If destination is 0 (insert at start), make new_head the head
        last->next = f->head;
        f->head = new_head;
    }

    f->line_count += (rng->end - rng->begin + 1); // Update line count
    printf("Lines copied\n");
    return 0;
}
int handleCommand(char *command, file *f, int not_dirty)
{
    int i = 0, argc = 0;
    char *argv[3] = {NULL, NULL, NULL}; // Initialize argv to NULL

    argv[argc++] = command; // First argument (command itself)

    while (command[i] != '\0')
    {

        if (command[i] == '\n')
        {
            command[i] = '\0'; // Null-terminate at the newline
            break;
        }
        else if (command[i] == ' ')
        {
            // printf("command: %s, argc: %d\n", argv[0], argc);
            //  check for every command that takes a line as last arg so I don't have to require quotes for strings
            //  to make this parser more versatile I would only make a new arg pointer if not in quoted string
            if (((strcmp(argv[0], "@END") == 0 || strcmp(argv[0], "FIND") == 0) && argc == 2) ||
                ((strcmp(argv[0], "ADD<") == 0 || strcmp(argv[0], "EDIT") == 0) && argc == 3))
            {
                i++;
                continue;
            }
            command[i] = '\0'; // Null-terminate the argument
            if (argc == 3)
            { // Ensure we do not exceed argv[2]
                printf("Too many arguments\n");
            }
            argv[argc++] = command + i + 1;
        }

        i++;
    }
    if (strcmp(argv[0], "PRINT") == 0)
    {
        printList(f->head);
        return not_dirty;
    }
    else if (strcmp(argv[0], "QUIT") == 0)
    {
        if (!not_dirty)
        {
            printf("File has been modified. Save before quitting? (Y/n) ");

            char response[2];
            read(0, response, sizeof(response));

            if (!(response[0] == 'N') && !(response[0] == 'n'))
            {
                save(f);
            }
        }
        close(f->fd);
        exit(0);
    }
    else if (strcmp(argv[0], "@END") == 0)
    {
        if (argc != 2)
        {
            printf("Usage: @END <line>\n");
            return not_dirty;
        }
        if (!not_dirty)
        {
            end(f, argv[1]);
            return 0;
        }
        else
        {
            return end(f, argv[1]);
        }
    }
    else if (strcmp(argv[0], "ADD<") == 0)
    {
        if (argc != 3)
        {
            printf("Usage: ADD< <line_number> <line>\n");
            return not_dirty;
        }
        if (!not_dirty)
        {
            add(f, argv[2], integer(argv[1]));
            return 0;
        }
        else
        {
            return add(f, argv[2], integer(argv[1]));
        }
    }
    else if (strcmp(argv[0], "SAVE") == 0)
    {
        if (argc == 2)
        {
            char *filename = f->name;
            f->name = argv[1];
            save(f);
            f->name = filename;
        }
        else
        {
            save(f);
            not_dirty = 1;
        }
        open(f->name, O_RDWR);
        return not_dirty;
    }
    else if (strcmp(argv[0], "EDIT") == 0)
    {
        if (argc != 3)
        {
            printf("Usage: EDIT <line_number> <line>\n");
            return not_dirty;
        }
        if (!not_dirty)
        {
            edit(f, argv[2], integer(argv[1]));
            return 0;
        }
        else
        {
            return edit(f, argv[2], integer(argv[1]));
        }
        edit(f, argv[2], integer(argv[1]));
        return 0;
    }
    else if (strcmp(argv[0], "LIST") == 0)
    {
        if (argc != 2)
        {
            printf("Usage: LIST <begin:end>\n");
            return not_dirty;
        }
        range r = parseRange(argv[1], f->line_count);
        list(f, r);
        return not_dirty;
    }
    else if (strcmp(argv[0], "DROP") == 0)
    {
        if (argc != 2)
        {
            printf("Usage: DROP <begin:end>\n");
            return not_dirty;
        }
        range r = parseRange(argv[1], f->line_count);
        printf("Are you sure you want to drop lines %d to %d? (y/N) ", r.begin, r.end);
        char response[2];
        read(0, response, sizeof(response));
        if (response[0] == 'Y' || response[0] == 'y')
        {
            if (!not_dirty)
            {
                drop(f, r);
                return 0;
            }
            else
            {
                return drop(f, r);
            }
        }
        return not_dirty;
    }
    else if (strcmp(argv[0], "OPEN") == 0)
    {
        if (argc != 2)
        {
            printf("Usage: OPEN <filename>\n");
            return not_dirty;
        }
        if (!not_dirty)
        {
            printf("File has been modified. Save before quitting? (Y/n) ");

            char response[2];
            read(0, response, sizeof(response));

            if (response[0] == 'Y' || response[0] == 'y')
            {
                save(f);
            }
        }
        close(f->fd);
        f->name = argv[1];
        f->fd = open(argv[1], O_RDWR); // if i need old fd store new fd in new int var
        if (f->fd < 0)
        {
            printf("edit: cannot open %s\n", argv[1]);
            exit(0);
        }
        file new_file = readFile(f->fd);
        new_file.name = argv[1];
        *f = new_file;
        printf("edit: %d lines (%d bytes) read from %s\n", f->line_count, f->total_bytes, argv[1]);
        return 1;
    }
    else if (strcmp(argv[0], "FIND") == 0)
    {
        if (argc != 2)
        {
            printf("Usage: FIND <string>\n");
            return not_dirty;
        }
        find(f, argv[1]);

        return not_dirty;
    }
    else if (strcmp(argv[0], "HELP") == 0)
    {
        if (argc > 2)
        {
            printf("Usage: HELP ?<command>\n");
            return not_dirty;
        }
        else if (argc == 1)
        {
            help(0);
        }
        else
        {
            help(argv[1]);
        }
    }
    else if (strcmp(argv[0], "COPY") == 0)
    {
        if (argc != 3)
        {
            printf("Usage: COPY <begin:end> <line_number>\n");
            return not_dirty;
        }
        range rng = parseRange(argv[1], f->line_count);
        int dest_line = integer(argv[2]);
        if (!not_dirty)
        {
            copy(f, &rng, dest_line - 1);
            return 0;
        }
        else
        {
            return copy(f, &rng, dest_line - 1);
        }
    }
    else
    {
        printf("Unknown command: %s. Try HELP to see commands.\n", command);
    }
    return not_dirty; // 1 means not dirty 0 means dirty
}
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: edit <filename>\n");
        exit(0);
    }

    int src_fd = open(argv[1], O_RDWR);
    if (src_fd < 0)
    {
        printf("edit: cannot open %s\n", argv[1]);
        exit(0);
    }

    printf("edit: Welcome to edit\n");
    int not_dirty = 1;
    file result = readFile(src_fd);
    result.name = argv[1];
    printf("edit: %d lines (%d bytes) read from %s\n", result.line_count, result.total_bytes, argv[1]);
    // Enter interactive mode with the edit prompt
    char command[100]; // Buffer for user input
    while (1)
    {
        printf("edit> ");

        // Read user input
        if (gets(command, sizeof(command)) == 0)
        {
            break; // Exit if user enters nothing
        }

        // Process the command
        not_dirty = handleCommand(command, &result, not_dirty);
    }
}
