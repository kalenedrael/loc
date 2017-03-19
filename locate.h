#ifndef _LOCATE_H_
#define _LOCATE_H_

void locate_init(int n_samples, int n_mics);
void locate_xcor(real_t **data, size_t offset, real_t *res);

#endif /* _LOCATE_H_ */
