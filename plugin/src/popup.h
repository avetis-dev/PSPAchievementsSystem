#ifndef POPUP_H
#define POPUP_H

void pach_popup_init(void);
void pach_popup_show(const char *title, const char *desc);
void pach_popup_update(void);
void pach_popup_draw_current(void);
int  pach_popup_is_active(void);

#endif /* POPUP_H */