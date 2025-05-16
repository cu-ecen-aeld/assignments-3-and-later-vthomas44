#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[]) {

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments. Usage: %s <file> <string>", argv[0]);
        fprintf(stderr, "Usage: %s <file> <string>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const char *string = argv[2];

    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to open file: %s", filename);
        perror("Error opening file");
        closelog();
        return 1;
    }

    if (fprintf(fp, "%s", string) < 0) {
        syslog(LOG_ERR, "Failed to write to file: %s", filename);
        perror("Error writing to file");
        fclose(fp);
        closelog();
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", string, filename);

    fclose(fp);
    closelog();
    return 0;
}
