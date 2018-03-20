#ifndef HID_H
#define HID_H
struct Report;

void debug_printf(const char *, ...);
int allocate_report(struct Report *, size_t);
int free_report(struct Report *);
void hid_read(int, struct Report *, int, report_desc_t);
int readloop(int, report_desc_t, int);
#endif
