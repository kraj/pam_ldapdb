#include <map>
#include <string>
#include <utility>

#define PAM_SM_AUTH
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#define LDAP_DEPRECATED 1
#include <ldap.h>

typedef std::map<std::string, std::string> ArgMap;

static int ldap_to_pam_rc(int ldap_rc)
{
    switch (ldap_rc) {
    case LDAP_SUCCESS:
        /* everything was fine */
        return PAM_SUCCESS;
    case LDAP_UNAVAILABLE:
    case LDAP_TIMELIMIT_EXCEEDED:
    case LDAP_OPERATIONS_ERROR:
    case LDAP_BUSY:
    case LDAP_LOOP_DETECT:
    case LDAP_SERVER_DOWN:
    case LDAP_TIMEOUT:
    case LDAP_CONNECT_ERROR:
    case LDAP_NO_RESULTS_RETURNED:
        /* cannot access LDAP correctly */
        return PAM_AUTHINFO_UNAVAIL;
    }

    /* something else went wrong */
    return PAM_AUTH_ERR;
}

static int verify(const char* host,
                  const char* binddn,
                  const char* pw)
{
    LDAP* ld;
    int ldap_rc, pam_rc;

    ldap_rc = ldap_initialize(&ld, host);
    pam_rc = ldap_to_pam_rc(ldap_rc);
    if (pam_rc != PAM_SUCCESS)
        return pam_rc;

    ldap_rc = ldap_simple_bind_s(ld, binddn, pw);
    return ldap_to_pam_rc(ldap_rc);
}


static ArgMap get_args(int argc,
                       const char** argv)
{
    ArgMap arguments;

    for (int i = 0; i < argc; i++) {
        std::string arg(argv[i]);
        std::string::size_type pos = arg.find("=");
        if (pos != std::string::npos) {
            std::string key = arg.substr(0, pos);
            std::string value = "";

            if (arg.length() > pos+1) {
                value = arg.substr(pos+1);
            }

            arguments.insert(std::make_pair(key, value));
        }
    }

    return arguments;
}


static void replace_all(std::string& s,
                        const std::string& search_s,
                        const std::string& replace_s)
{
    std::string::size_type last_pos = 0;
    while ((last_pos = s.find(search_s, last_pos)) != std::string::npos) {
        s.replace(last_pos, search_s.length(), replace_s);
    }
}


PAM_EXTERN int pam_sm_authenticate(pam_handle_t* pamh,
                                   int flags,
                                   int argc,
                                   const char** argv)
{
    const char* user;
    const char* pass;
    ArgMap arguments = get_args(argc, argv);

    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS) {
        return PAM_AUTH_ERR;
    }
    if (pam_get_authtok(pamh, PAM_AUTHTOK, &pass, NULL) != PAM_SUCCESS) {
        return PAM_AUTH_ERR;
    }

    // ensure uri and binddn PAM parameters were specified
    if (arguments.find("uri") == arguments.end() ||
        arguments.find("binddn") == arguments.end()) {
        return PAM_AUTH_ERR;
    }

    std::string binddn = arguments["binddn"];
    replace_all(binddn, "%s", user);

    // check against ldap database
    return verify(arguments["uri"].c_str(), binddn.c_str(), pass);
}


PAM_EXTERN int pam_sm_setcred(pam_handle_t* pamh,
                              int flags,
                              int argc,
                              const char** argv)
{
    return PAM_SUCCESS;
}
