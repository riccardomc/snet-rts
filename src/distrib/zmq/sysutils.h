#ifndef DISTRIBZMQSYSUT_H
#define DISTRIBZMQSYSUT_H

/*
* FIXME: consider moving this header to src/util/...
*/

/*
* Retrieve hostname from system
*/
char *SNetUtilSysHostname();

/*
* Check if envname environment variable is set.
*/
bool SNetUtilSysEnvInt(char *envname, int *parm);
bool SNetUtilSysEnvStr(char *envname, char *parm, size_t size);

#endif /* DISTRIBZMQSYSUT_T */
