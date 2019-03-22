#include "multipart.h"
#include <string.h>
#include <esp_log.h>

/* 
    https://tools.ietf.org/html/rfc7578
**/

static const char *TAG = "MLTPRT";

// 
static const char s_content_disposition_str[] = "Content-Disposition";
static const size_t s_content_disposition_str_len = sizeof(s_content_disposition_str) - sizeof(char);
static const char s_formdata_name_str[] = ": form-data; name=\"";
static const size_t s_formdata_name_str_len = sizeof(s_formdata_name_str) - sizeof(char);
static const char s_filename_str[] = "; filename=\"";
static const size_t s_filename_str_len = sizeof(s_filename_str) - sizeof(char);
static const char s_crlf_str[] = "\r\n";
static const size_t s_crlf_str_len = sizeof(s_crlf_str) - sizeof(char);
static const char s_content_type_str[] = "Content-Type";
static const size_t s_content_type_str_len = sizeof(s_content_type_str) - sizeof(char);
static const char s_boundary_hyphen_str[] = "--";
static const size_t s_boundary_hyphen_str_len = sizeof(s_boundary_hyphen_str) - sizeof(char);
static const char s_boundary_str[] = "boundary=";
static const char s_field_parameter_end_marker = '\"';
static const char s_header_field_separator = ':';

// 
static esp_err_t multipart_parse_header_line(multipart_post_t* post, const char* line, bool* is_content_disposition);
static size_t multipart_find_char(const char* str, const char c);
static size_t multipart_find_str(const char* str1, const char* str2);
static inline bool multipart_check_crlf(const char* pos);

esp_err_t multipart_init(multipart_parse_context_t* context, const char* header) {
    char* b = NULL;

    // Find the boundary in the header field.
    if ((b = strstr(header, s_boundary_str)) != NULL) {
        // The boundary MUST be prefixed with --.
        size_t chars = snprintf(context->b, MULTIPART_MAX_BOUNDARY_LEN, "%s%s", s_boundary_hyphen_str, b + 9); // Skip "boudary=" characters.
        context->b_len = chars;

        return ESP_OK;
    }
    ESP_LOGE(TAG, "No boundary found in header.");

    return ESP_FAIL;
}

esp_err_t multipart_parse(const multipart_parse_context_t* context, const char* data, multipart_post_t* post, char** next) {
    char* cur = NULL;
    char* tmp = NULL;
    size_t len = 0;

    if(context == NULL || post == NULL) {
        ESP_LOGE(TAG, "Invalid params.");
        return ESP_FAIL;
    }

    // Search for boundary.
    if((cur = strstr(data, context->b)) != NULL ) {
        // Skip index to end of boundary.
        cur = cur + context->b_len; 

        // Boundary MUST end with CRLF.
        if(!multipart_check_crlf(cur)) {
            ESP_LOGE(TAG, "Boundary line does not end with CRLF.");
            return ESP_FAIL;
        }

        cur = cur + s_crlf_str_len;

        // Iterate over all header fields.
        bool has_content_disposition = false;
        while((len = multipart_find_str(cur, s_crlf_str)) > 0) {
            if(multipart_parse_header_line(post, cur, &has_content_disposition) != ESP_OK) {
                return ESP_FAIL;
            }

            cur = cur + len;
            if(!multipart_check_crlf(cur)) {
                ESP_LOGI(TAG, "Invalid header field, missing CRLF.");
                return ESP_FAIL;
            }

            cur = cur + s_crlf_str_len;
        }

        // The content disposition field MUST be included.
        if(has_content_disposition == false) {
            ESP_LOGE(TAG, "Content disposition is missing or invalid.");
            return ESP_FAIL;
        }
        
        // Start of actual data MUST be marked with an extra CRLR.
        if(!multipart_check_crlf(cur)) {
            ESP_LOGE(TAG, "Could not find start of data (missing CRLF).");
            return ESP_FAIL;
        }

        cur = cur + s_crlf_str_len;

        // Store start position of post value.
        tmp = cur;

        // Data part MUST end with CRLF followed by the boundary.
        // First find boundary.
        len = multipart_find_str(cur, context->b);
        if(len == 0) {
            ESP_LOGE(TAG, "Missing end boundary.");
            return ESP_FAIL;
        }

        // Reverse check for CRLF.
        if(!multipart_check_crlf(cur + len - s_crlf_str_len)) {
            ESP_LOGE(TAG, "Could not find start of data (missing CRLF).");
            return ESP_FAIL;
        }

        post->data = tmp;
        post->data_len = len - s_crlf_str_len;

        if(next != NULL) {
            // The last boundary MUST end with "<boundary>--", note the extra --..
            if(*(cur + len + context->b_len) == s_boundary_hyphen_str[0] && *(cur + len + context->b_len + 1) == s_boundary_hyphen_str[1]) {
                *next = NULL;
            } else {
                *next = cur + len - s_crlf_str_len;
            }
        }

        return ESP_OK;
    }

    ESP_LOGE(TAG, "Could not find boundary.");

    return ESP_FAIL;
}

static esp_err_t multipart_parse_header_line(multipart_post_t* post, const char* line, bool* is_content_disposition) {
    char* tmp = (char*)line;

    if(post == NULL || line == NULL || is_content_disposition == NULL) {
        ESP_LOGE(TAG, "Invalid parameters.");
        return ESP_FAIL;
    }

    size_t len = multipart_find_char(tmp, s_header_field_separator);
    if(len == 0) {
        ESP_LOGE(TAG, "Invalid header field.");
        return ESP_FAIL;
    }

    if(strncmp(tmp, s_content_disposition_str, len) == 0) {
        // Move to first char after : (value part).
        tmp = tmp + len;

        // Content disposition MUST be form-data and MUST contain a name field.
        tmp = strstr(tmp, s_formdata_name_str);
        if(tmp == NULL) {
            ESP_LOGE(TAG, "Content disposition field invalid.");
            return ESP_FAIL;
        }
        tmp = tmp + s_formdata_name_str_len;

        // Name field ends with ".
        len = multipart_find_char(tmp, s_field_parameter_end_marker);
        if(len == 0) {
            return ESP_FAIL;
        }

        snprintf(post->name, MULTIPART_MAX_POST_NAME_LEN, "%.*s", len, tmp);

        *is_content_disposition = true;

        // Move.
        tmp = tmp + len + 1;

        // If the content is a file, the filename parameter SHOULD be included.
        tmp = strstr(tmp, s_filename_str);
        if(tmp != NULL) {
            tmp = tmp + s_filename_str_len;
            len = multipart_find_char(tmp, s_field_parameter_end_marker);
            if(len > 0) {
                snprintf(post->filename, MULTIPART_MAX_POST_FILENAME_LEN, "%.*s", len, tmp);
            }
        }
    } else if(strncmp(tmp, s_content_type_str, len) == 0) {
        // Move to first char after : (value part).
        tmp = tmp + len;

        // Search for CRLF, everything in between is the content type.
        size_t len = multipart_find_str(tmp, s_crlf_str);
        if(len == 0) {
            ESP_LOGE(TAG, "Content type field invalid.");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Content-type: %.*s", len, tmp);
        snprintf(post->content_type, MULTIPART_MAX_CONTENT_TYPE_LEN, "%.*s", len, tmp);
    } else {
        ESP_LOGI(TAG, "Ignoring header %.*s.", len, tmp);
    }

    return ESP_OK;
}

static size_t multipart_find_char(const char* str, const char c) {
    size_t i = 0;

    while(str[i] != '\0') {
        if(str[i] == c) {
            return i;
        }

        i++;
    }

    return -1;
}

static size_t multipart_find_str(const char* str1, const char* str2) {
    size_t i = 0;
    size_t j = 0;
    size_t k = 0;

    while(str1[i] != '\0') {
        if(str1[i] == str2[j]) {
            if(j == 0) {
                k = i;
            }

            j++;

            if(str2[j] == '\0') {
                return k;
            }
        } else {
            j = 0;
        }

        i++;
    }

    return -1;
}

static inline bool multipart_check_crlf(const char* pos) {
    if(*(pos) != s_crlf_str[0] || *(pos + 1) != s_crlf_str[1]) {
        return false;
    }

    return true;
}