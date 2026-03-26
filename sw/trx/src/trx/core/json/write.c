#include <trx/core/json/write.h>

#include <trx/core/memory.h>

static int M_GetValueSize_Minified(const JSON_VALUE *value, size_t *size);
static char *M_WriteValue_Minified(const JSON_VALUE *value, char *data);

static int M_GetValueSize_Pretty(
    const JSON_VALUE *value, size_t depth, size_t indent_size,
    size_t newline_size, size_t *size);
static char *M_WriteValue_Pretty(
    const JSON_VALUE *value, size_t depth, const char *indent,
    const char *newline, char *data);

static int M_GetNumberSize(const JSON_NUMBER *number, size_t *size)
{
    json_uintmax_t parsed_number;
    size_t i;

    if (number->number_size >= 2) {
        switch (number->number[1]) {
        case 'x':
        case 'X':
            /* the number is a JSON_PARSE_FLAGS_ALLOW_HEXADECIMAL_NUMBERS
             * hexadecimal so we have to do extra work to convert it to a
             * non-hexadecimal for JSON output. */
            parsed_number = json_strtoumax(number->number, nullptr, 0);

            i = 0;

            while (0 != parsed_number) {
                parsed_number /= 10;
                i++;
            }

            *size += i;
            return 0;
        }
    }

    /* check to see if the number has leading/trailing decimal point. */
    i = 0;

    /* skip any leading '+' or '-'. */
    if ((i < number->number_size)
        && (('+' == number->number[i]) || ('-' == number->number[i]))) {
        i++;
    }

    /* check if we have infinity. */
    if ((i < number->number_size) && ('I' == number->number[i])) {
        const char *inf = "Infinity";
        size_t k;

        for (k = i; k < number->number_size; k++) {
            const char c = *inf++;

            /* Check if we found the Infinity string! */
            if ('\0' == c) {
                break;
            } else if (c != number->number[k]) {
                break;
            }
        }

        if ('\0' == *inf) {
            /* Inf becomes 1.7976931348623158e308 because JSON can't support it.
             */
            *size += 22;

            /* if we had a leading '-' we need to record it in the JSON output.
             */
            if ('-' == number->number[0]) {
                *size += 1;
            }
        }

        return 0;
    }

    /* check if we have nan. */
    if ((i < number->number_size) && ('N' == number->number[i])) {
        const char *nan = "NaN";
        size_t k;

        for (k = i; k < number->number_size; k++) {
            const char c = *nan++;

            /* Check if we found the NaN string! */
            if ('\0' == c) {
                break;
            } else if (c != number->number[k]) {
                break;
            }
        }

        if ('\0' == *nan) {
            /* NaN becomes 1 because JSON can't support it. */
            *size += 1;

            return 0;
        }
    }

    /* if we had a leading decimal point. */
    if ((i < number->number_size) && ('.' == number->number[i])) {
        /* 1 + because we had a leading decimal point. */
        *size += 1;
        goto cleanup;
    }

    for (; i < number->number_size; i++) {
        const char c = number->number[i];
        if (!('0' <= c && c <= '9')) {
            break;
        }
    }

    /* if we had a trailing decimal point. */
    if ((i + 1 == number->number_size) && ('.' == number->number[i])) {
        /* 1 + because we had a trailing decimal point. */
        *size += 1;
        goto cleanup;
    }

cleanup:
    *size += number->number_size; /* the actual string of the number. */

    /* if we had a leading '+' we don't record it in the JSON output. */
    if ('+' == number->number[0]) {
        *size -= 1;
    }

    return 0;
}

static int M_GetStringSize(const JSON_STRING *string, size_t *size)
{
    size_t i;
    for (i = 0; i < string->string_size; i++) {
        switch (string->string[i]) {
        case '"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            *size += 2;
            break;
        default:
            *size += 1;
            break;
        }
    }

    *size += 2; /* need to encode the surrounding '"' characters. */

    return 0;
}

static int M_GetArraySize_Minified(const JSON_ARRAY *array, size_t *size)
{
    JSON_ARRAY_ELEMENT *element;

    *size += 2; /* '[' and ']'. */

    if (1 < array->length) {
        *size += array->length - 1; /* ','s seperate each element. */
    }

    for (element = array->start; nullptr != element; element = element->next) {
        if (M_GetValueSize_Minified(element->value, size)) {
            /* value was malformed! */
            return 1;
        }
    }

    return 0;
}

static int M_GetObjectSize_Minified(const JSON_OBJECT *object, size_t *size)
{
    JSON_OBJECT_ELEMENT *element;

    *size += 2; /* '{' and '}'. */

    *size += object->length; /* ':'s seperate each name/value pair. */

    if (1 < object->length) {
        *size += object->length - 1; /* ','s seperate each element. */
    }

    for (element = object->start; nullptr != element; element = element->next) {
        if (M_GetStringSize(element->name, size)) {
            /* string was malformed! */
            return 1;
        }

        if (M_GetValueSize_Minified(element->value, size)) {
            /* value was malformed! */
            return 1;
        }
    }

    return 0;
}

static int M_GetValueSize_Minified(const JSON_VALUE *value, size_t *size)
{
    switch (value->type) {
    default:
        /* unknown value type found! */
        return 1;
    case JSON_TYPE_NUMBER:
        return M_GetNumberSize((JSON_NUMBER *)value->payload, size);
    case JSON_TYPE_STRING:
        return M_GetStringSize((JSON_STRING *)value->payload, size);
    case JSON_TYPE_ARRAY:
        return M_GetArraySize_Minified((JSON_ARRAY *)value->payload, size);
    case JSON_TYPE_OBJECT:
        return M_GetObjectSize_Minified((JSON_OBJECT *)value->payload, size);
    case JSON_TYPE_TRUE:
        *size += 4; /* the string "true". */
        return 0;
    case JSON_TYPE_FALSE:
        *size += 5; /* the string "false". */
        return 0;
    case JSON_TYPE_NULL:
        *size += 4; /* the string "null". */
        return 0;
    }
}

static char *M_WriteNumber(const JSON_NUMBER *number, char *data)
{
    json_uintmax_t parsed_number, backup;
    size_t i;

    if (number->number_size >= 2) {
        switch (number->number[1]) {
        case 'x':
        case 'X':
            /* The number is a JSON_PARSE_FLAGS_ALLOW_HEXADECIMAL_NUMBERS
             * hexadecimal so we have to do extra work to convert it to a
             * non-hexadecimal for JSON output. */
            parsed_number = json_strtoumax(number->number, nullptr, 0);

            /* We need a copy of parsed number twice, so take a backup of it. */
            backup = parsed_number;

            i = 0;

            while (0 != parsed_number) {
                parsed_number /= 10;
                i++;
            }

            /* Restore parsed_number to its original value stored in the backup.
             */
            parsed_number = backup;

            /* Now use backup to take a copy of i, or the length of the string.
             */
            backup = i;

            do {
                *(data + i - 1) = '0' + (char)(parsed_number % 10);
                parsed_number /= 10;
                i--;
            } while (0 != parsed_number);

            data += backup;

            return data;
        }
    }

    /* check to see if the number has leading/trailing decimal point. */
    i = 0;

    /* skip any leading '-'. */
    if ((i < number->number_size)
        && (('+' == number->number[i]) || ('-' == number->number[i]))) {
        i++;
    }

    /* check if we have infinity. */
    if ((i < number->number_size) && ('I' == number->number[i])) {
        const char *inf = "Infinity";
        size_t k;

        for (k = i; k < number->number_size; k++) {
            const char c = *inf++;

            /* Check if we found the Infinity string! */
            if ('\0' == c) {
                break;
            } else if (c != number->number[k]) {
                break;
            }
        }

        if ('\0' == *inf++) {
            const char *dbl_max;

            /* if we had a leading '-' we need to record it in the JSON output.
             */
            if ('-' == number->number[0]) {
                *data++ = '-';
            }

            /* Inf becomes 1.7976931348623158e308 because JSON can't support it.
             */
            for (dbl_max = "1.7976931348623158e308"; '\0' != *dbl_max;
                 dbl_max++) {
                *data++ = *dbl_max;
            }

            return data;
        }
    }

    /* check if we have nan. */
    if ((i < number->number_size) && ('N' == number->number[i])) {
        const char *nan = "NaN";
        size_t k;

        for (k = i; k < number->number_size; k++) {
            const char c = *nan++;

            /* Check if we found the NaN string! */
            if ('\0' == c) {
                break;
            } else if (c != number->number[k]) {
                break;
            }
        }

        if ('\0' == *nan++) {
            /* NaN becomes 0 because JSON can't support it. */
            *data++ = '0';
            return data;
        }
    }

    /* if we had a leading decimal point. */
    if ((i < number->number_size) && ('.' == number->number[i])) {
        i = 0;

        /* skip any leading '+'. */
        if ('+' == number->number[i]) {
            i++;
        }

        /* output the leading '-' if we had one. */
        if ('-' == number->number[i]) {
            *data++ = '-';
            i++;
        }

        /* insert a '0' to fix the leading decimal point for JSON output. */
        *data++ = '0';

        /* and output the rest of the number as normal. */
        for (; i < number->number_size; i++) {
            *data++ = number->number[i];
        }

        return data;
    }

    for (; i < number->number_size; i++) {
        const char c = number->number[i];
        if (!('0' <= c && c <= '9')) {
            break;
        }
    }

    /* if we had a trailing decimal point. */
    if ((i + 1 == number->number_size) && ('.' == number->number[i])) {
        i = 0;

        /* skip any leading '+'. */
        if ('+' == number->number[i]) {
            i++;
        }

        /* output the leading '-' if we had one. */
        if ('-' == number->number[i]) {
            *data++ = '-';
            i++;
        }

        /* and output the rest of the number as normal. */
        for (; i < number->number_size; i++) {
            *data++ = number->number[i];
        }

        /* insert a '0' to fix the trailing decimal point for JSON output. */
        *data++ = '0';

        return data;
    }

    i = 0;

    /* skip any leading '+'. */
    if ('+' == number->number[i]) {
        i++;
    }

    for (; i < number->number_size; i++) {
        *data++ = number->number[i];
    }

    return data;
}

static char *M_WriteString(const JSON_STRING *string, char *data)
{
    size_t i;

    *data++ = '"'; /* open the string. */

    for (i = 0; i < string->string_size; i++) {
        switch (string->string[i]) {
        case '"':
            *data++ = '\\'; /* escape the control character. */
            *data++ = '"';
            break;
        case '\\':
            *data++ = '\\'; /* escape the control character. */
            *data++ = '\\';
            break;
        case '\b':
            *data++ = '\\'; /* escape the control character. */
            *data++ = 'b';
            break;
        case '\f':
            *data++ = '\\'; /* escape the control character. */
            *data++ = 'f';
            break;
        case '\n':
            *data++ = '\\'; /* escape the control character. */
            *data++ = 'n';
            break;
        case '\r':
            *data++ = '\\'; /* escape the control character. */
            *data++ = 'r';
            break;
        case '\t':
            *data++ = '\\'; /* escape the control character. */
            *data++ = 't';
            break;
        default:
            *data++ = string->string[i];
            break;
        }
    }

    *data++ = '"'; /* close the string. */

    return data;
}

static char *M_WriteArray_Minified(const JSON_ARRAY *array, char *data)
{
    JSON_ARRAY_ELEMENT *element = nullptr;

    *data++ = '['; /* open the array. */

    for (element = array->start; nullptr != element; element = element->next) {
        if (element != array->start) {
            *data++ = ','; /* ','s seperate each element. */
        }

        data = M_WriteValue_Minified(element->value, data);

        if (nullptr == data) {
            /* value was malformed! */
            return nullptr;
        }
    }

    *data++ = ']'; /* close the array. */

    return data;
}

static char *M_WriteObject_Minified(const JSON_OBJECT *object, char *data)
{
    JSON_OBJECT_ELEMENT *element = nullptr;

    *data++ = '{'; /* open the object. */

    for (element = object->start; nullptr != element; element = element->next) {
        if (element != object->start) {
            *data++ = ','; /* ','s seperate each element. */
        }

        data = M_WriteString(element->name, data);

        if (nullptr == data) {
            /* string was malformed! */
            return nullptr;
        }

        *data++ = ':'; /* ':'s seperate each name/value pair. */

        data = M_WriteValue_Minified(element->value, data);

        if (nullptr == data) {
            /* value was malformed! */
            return nullptr;
        }
    }

    *data++ = '}'; /* close the object. */

    return data;
}

static char *M_WriteValue_Minified(const JSON_VALUE *value, char *data)
{
    switch (value->type) {
    default:
        /* unknown value type found! */
        return nullptr;
    case JSON_TYPE_NUMBER:
        return M_WriteNumber((JSON_NUMBER *)value->payload, data);
    case JSON_TYPE_STRING:
        return M_WriteString((JSON_STRING *)value->payload, data);
    case JSON_TYPE_ARRAY:
        return M_WriteArray_Minified((JSON_ARRAY *)value->payload, data);
    case JSON_TYPE_OBJECT:
        return M_WriteObject_Minified((JSON_OBJECT *)value->payload, data);
    case JSON_TYPE_TRUE:
        data[0] = 't';
        data[1] = 'r';
        data[2] = 'u';
        data[3] = 'e';
        return data + 4;
    case JSON_TYPE_FALSE:
        data[0] = 'f';
        data[1] = 'a';
        data[2] = 'l';
        data[3] = 's';
        data[4] = 'e';
        return data + 5;
    case JSON_TYPE_NULL:
        data[0] = 'n';
        data[1] = 'u';
        data[2] = 'l';
        data[3] = 'l';
        return data + 4;
    }
}

void *JSON_WriteMinified(const JSON_VALUE *value, size_t *out_size)
{
    size_t size = 0;
    char *data = nullptr;
    char *data_end = nullptr;

    if (nullptr == value) {
        return nullptr;
    }

    if (M_GetValueSize_Minified(value, &size)) {
        /* value was malformed! */
        return nullptr;
    }

    size += 1; /* for the '\0' null terminating character. */

    data = (char *)Memory_Alloc(size);

    if (nullptr == data) {
        /* malloc failed! */
        return nullptr;
    }

    data_end = M_WriteValue_Minified(value, data);

    if (nullptr == data_end) {
        /* bad chi occurred! */
        Memory_Free(data);
        return nullptr;
    }

    /* null terminated the string. */
    *data_end = '\0';

    if (nullptr != out_size) {
        *out_size = size;
    }

    return data;
}

static int M_GetArraySize_Pretty(
    const JSON_ARRAY *array, size_t depth, size_t indent_size,
    size_t newline_size, size_t *size)
{
    JSON_ARRAY_ELEMENT *element;

    *size += 1; /* '['. */

    if (0 < array->length) {
        /* if we have any elements we need to add a newline after our '['. */
        *size += newline_size;

        *size += array->length - 1; /* ','s seperate each element. */

        for (element = array->start; nullptr != element;
             element = element->next) {
            /* each element gets an indent. */
            *size += (depth + 1) * indent_size;

            if (M_GetValueSize_Pretty(
                    element->value, depth + 1, indent_size, newline_size,
                    size)) {
                /* value was malformed! */
                return 1;
            }

            /* each element gets a newline too. */
            *size += newline_size;
        }

        /* since we wrote out some elements, need to add a newline and
         * indentation.
         */
        /* to the trailing ']'. */
        *size += depth * indent_size;
    }

    *size += 1; /* ']'. */

    return 0;
}

static int M_GetObjectSize_Pretty(
    const JSON_OBJECT *object, size_t depth, size_t indent_size,
    size_t newline_size, size_t *size)
{
    JSON_OBJECT_ELEMENT *element;

    *size += 1; /* '{'. */

    if (0 < object->length) {
        *size += newline_size; /* need a newline next. */

        *size += object->length - 1; /* ','s seperate each element. */

        for (element = object->start; nullptr != element;
             element = element->next) {
            /* each element gets an indent and newline. */
            *size += (depth + 1) * indent_size;
            *size += newline_size;

            if (M_GetStringSize(element->name, size)) {
                /* string was malformed! */
                return 1;
            }

            *size += 2; /* seperate each name/value pair with ": ". */

            if (M_GetValueSize_Pretty(
                    element->value, depth + 1, indent_size, newline_size,
                    size)) {
                /* value was malformed! */
                return 1;
            }
        }

        *size += depth * indent_size;
    }

    *size += 1; /* '}'. */

    return 0;
}

static int M_GetValueSize_Pretty(
    const JSON_VALUE *value, size_t depth, size_t indent_size,
    size_t newline_size, size_t *size)
{
    switch (value->type) {
    default:
        /* unknown value type found! */
        return 1;
    case JSON_TYPE_NUMBER:
        return M_GetNumberSize((JSON_NUMBER *)value->payload, size);
    case JSON_TYPE_STRING:
        return M_GetStringSize((JSON_STRING *)value->payload, size);
    case JSON_TYPE_ARRAY:
        return M_GetArraySize_Pretty(
            (JSON_ARRAY *)value->payload, depth, indent_size, newline_size,
            size);
    case JSON_TYPE_OBJECT:
        return M_GetObjectSize_Pretty(
            (JSON_OBJECT *)value->payload, depth, indent_size, newline_size,
            size);
    case JSON_TYPE_TRUE:
        *size += 4; /* the string "true". */
        return 0;
    case JSON_TYPE_FALSE:
        *size += 5; /* the string "false". */
        return 0;
    case JSON_TYPE_NULL:
        *size += 4; /* the string "null". */
        return 0;
    }
}

static char *M_WriteArray_Pretty(
    const JSON_ARRAY *array, size_t depth, const char *indent,
    const char *newline, char *data)
{
    size_t k, m;
    JSON_ARRAY_ELEMENT *element;

    *data++ = '['; /* open the array. */

    if (0 < array->length) {
        for (k = 0; '\0' != newline[k]; k++) {
            *data++ = newline[k];
        }

        for (element = array->start; nullptr != element;
             element = element->next) {
            if (element != array->start) {
                *data++ = ','; /* ','s seperate each element. */

                for (k = 0; '\0' != newline[k]; k++) {
                    *data++ = newline[k];
                }
            }

            for (k = 0; k < depth + 1; k++) {
                for (m = 0; '\0' != indent[m]; m++) {
                    *data++ = indent[m];
                }
            }

            data = M_WriteValue_Pretty(
                element->value, depth + 1, indent, newline, data);

            if (nullptr == data) {
                /* value was malformed! */
                return nullptr;
            }
        }

        for (k = 0; '\0' != newline[k]; k++) {
            *data++ = newline[k];
        }

        for (k = 0; k < depth; k++) {
            for (m = 0; '\0' != indent[m]; m++) {
                *data++ = indent[m];
            }
        }
    }

    *data++ = ']'; /* close the array. */

    return data;
}

static char *M_WriteObject_Pretty(
    const JSON_OBJECT *object, size_t depth, const char *indent,
    const char *newline, char *data)
{
    size_t k, m;
    JSON_OBJECT_ELEMENT *element;

    *data++ = '{'; /* open the object. */

    if (0 < object->length) {
        for (k = 0; '\0' != newline[k]; k++) {
            *data++ = newline[k];
        }

        for (element = object->start; nullptr != element;
             element = element->next) {
            if (element != object->start) {
                *data++ = ','; /* ','s seperate each element. */

                for (k = 0; '\0' != newline[k]; k++) {
                    *data++ = newline[k];
                }
            }

            for (k = 0; k < depth + 1; k++) {
                for (m = 0; '\0' != indent[m]; m++) {
                    *data++ = indent[m];
                }
            }

            data = M_WriteString(element->name, data);

            if (nullptr == data) {
                /* string was malformed! */
                return nullptr;
            }

            /* ": "s seperate each name/value pair. */
            *data++ = ':';
            *data++ = ' ';

            data = M_WriteValue_Pretty(
                element->value, depth + 1, indent, newline, data);

            if (nullptr == data) {
                /* value was malformed! */
                return nullptr;
            }
        }

        for (k = 0; '\0' != newline[k]; k++) {
            *data++ = newline[k];
        }

        for (k = 0; k < depth; k++) {
            for (m = 0; '\0' != indent[m]; m++) {
                *data++ = indent[m];
            }
        }
    }

    *data++ = '}'; /* close the object. */

    return data;
}

static char *M_WriteValue_Pretty(
    const JSON_VALUE *value, size_t depth, const char *indent,
    const char *newline, char *data)
{
    switch (value->type) {
    default:
        /* unknown value type found! */
        return nullptr;
    case JSON_TYPE_NUMBER:
        return M_WriteNumber((JSON_NUMBER *)value->payload, data);
    case JSON_TYPE_STRING:
        return M_WriteString((JSON_STRING *)value->payload, data);
    case JSON_TYPE_ARRAY:
        return M_WriteArray_Pretty(
            (JSON_ARRAY *)value->payload, depth, indent, newline, data);
    case JSON_TYPE_OBJECT:
        return M_WriteObject_Pretty(
            (JSON_OBJECT *)value->payload, depth, indent, newline, data);
    case JSON_TYPE_TRUE:
        data[0] = 't';
        data[1] = 'r';
        data[2] = 'u';
        data[3] = 'e';
        return data + 4;
    case JSON_TYPE_FALSE:
        data[0] = 'f';
        data[1] = 'a';
        data[2] = 'l';
        data[3] = 's';
        data[4] = 'e';
        return data + 5;
    case JSON_TYPE_NULL:
        data[0] = 'n';
        data[1] = 'u';
        data[2] = 'l';
        data[3] = 'l';
        return data + 4;
    }
}

void *JSON_WritePretty(
    const JSON_VALUE *value, const char *indent, const char *newline,
    size_t *out_size)
{
    size_t size = 0;
    size_t indent_size = 0;
    size_t newline_size = 0;
    char *data = nullptr;
    char *data_end = nullptr;

    if (nullptr == value) {
        return nullptr;
    }

    if (nullptr == indent) {
        indent = "  "; /* default to two spaces. */
    }

    if (nullptr == newline) {
        newline = "\n"; /* default to linux newlines. */
    }

    while ('\0' != indent[indent_size]) {
        ++indent_size; /* skip non-null terminating characters. */
    }

    while ('\0' != newline[newline_size]) {
        ++newline_size; /* skip non-null terminating characters. */
    }

    if (M_GetValueSize_Pretty(value, 0, indent_size, newline_size, &size)) {
        /* value was malformed! */
        return nullptr;
    }

    size += 1; /* for the '\0' null terminating character. */

    data = (char *)Memory_Alloc(size);

    if (nullptr == data) {
        /* malloc failed! */
        return nullptr;
    }

    data_end = M_WriteValue_Pretty(value, 0, indent, newline, data);

    if (nullptr == data_end) {
        /* bad chi occurred! */
        Memory_Free(data);
        return nullptr;
    }

    /* null terminated the string. */
    *data_end = '\0';

    if (nullptr != out_size) {
        *out_size = size;
    }

    return data;
}
