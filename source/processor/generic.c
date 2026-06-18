#include "../../include/processor.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/language.h"
#include "../../include/random.h"
#include "../../include/tables.h"
#include "../../include/time.h"

#include <string.h>

enum ResourceNormalizeSlashState
{
    RNS_WAIT_FIRST_NON_SLASH,
    RNS_CONSUME_FIRST_CHARACTERS,
    RNS_CONSUME_REST_CHARACTERS
};

enum ResourceNormalizeQuestionState
{
    RNQ_WAIT_FIRST_NON_SLASH,
    RNQ_CONSUME_FIRST_CHARACTERS,
    RNQ_CONSUME_REST_CHARACTERS,
    RNQ_MATCH_LANGUAGE,
    RNQ_CONSUME_LANGUAGE_FIRST_CHARACTERS
};

static void memset_alternating(char *destination, char c1, char c2, size_t n)
{
    if (c1 == c2)
    {
        memset(destination, c1, n);
    }
    else
    {
        /* Lagrange can't handle long sequences of equal symbols */
        size_t i;
        for (i = 0; i < n; i++) destination[i] = (i & 1) ? c1 : c2;
    }
}

static void trim_leading_shash(struct ConstantValue *resource)
{
    unsigned char i;
    for (i = 0;; resource->parts[i].p++, resource->parts[i].size--)
    {
        char c;
        while (resource->parts[i].size == 0) { i++; if (i == VALUE_PARTS) return; }
        c = *resource->parts[i].p;
        if (c != '/') break;
    }
}

static void trim_trailing_shash(struct ConstantValue *resource)
{
    unsigned char i;
    for (i = VALUE_PARTS - 1;; resource->parts[i].size--)
    {
        char c;
        while (resource->parts[i].size == 0) { if (i == 0) return; i--; }
        c = resource->parts[i].p[resource->parts[i].size - 1];
        if (c != '/') break;
    }
}

static void processor_generic_normalize_slash(struct ConstantValue *resource, char *language)
{
    char language_str[MAX_LANGUAGE_SIZE];
    unsigned char language_size = 0;
    struct ConstantValue local_resource;
    unsigned char i;

    /* Pop  last symbols */
    trim_leading_shash(resource);
    trim_trailing_shash(resource);
    local_resource = *resource;
    for (i = VALUE_PARTS - 1;;)
    {
        char c;
        while (local_resource.parts[i].size == 0) { if (i == 0) goto breakbreak; i--; }
        c = local_resource.parts[i].p[local_resource.parts[i].size - 1];
        resource->parts[i].size--;
        if (c == '/') break;
        if (language_size == MAX_LANGUAGE_SIZE) { *language = '\0'; return; } /* MAX_LANGUAGE_SIZE exceeded, it is definitely not a language */
        language_size++;
        language_str[MAX_LANGUAGE_SIZE - language_size] = c;
        resource->parts[i].size--;
    }
    breakbreak:
    *language = language_get(&language_str[MAX_LANGUAGE_SIZE - language_size], language_size);
    if (*language == '\0') return; /* Not a language */

    /* Language, accept local_resource */
    *resource = local_resource;
    trim_trailing_shash(resource);
}

static void processor_generic_normalize_question(struct ConstantValue *resource, char *language)
{
    const char lang[] = "?lang=";
    const unsigned char lang_size = sizeof(lang) - 1;
    char language_str[MAX_LANGUAGE_SIZE];
    unsigned char language_size = 0;
    struct ConstantValue local_resource;
    unsigned char i;
    
    /* Search for ? or # */
    trim_leading_shash(resource);
    local_resource = *resource;
    for (i = 0;; local_resource.parts[i].p++, local_resource.parts[i].size--)
    {
        char c;
        while (local_resource.parts[i].size == 0) { i++; if (i == VALUE_PARTS) { trim_trailing_shash(resource); *language = '\0'; return; } } /* Not found */
        c = *local_resource.parts[i].p;
        if (c == '#' || c == '?')
        {
            value_first(resource, value_size(resource) - value_size(&local_resource));
            trim_trailing_shash(resource);
            if (c == '#') { *language = '\0'; return; }
            break;
        }
    }

    /* Parse ? */
    language_size = 0;
    for (i = 0;; local_resource.parts[i].p++, local_resource.parts[i].size--)
    {
        char c;
        while (local_resource.parts[i].size == 0) { i++; if (i == VALUE_PARTS) { *language = '\0'; return; } } /* Not found */
        c = *local_resource.parts[i].p;
        if (c == '?')
        {
            language_size = 1;
        }
        else if (language_size < lang_size)
        {
            if (c == lang[language_size]) language_size++;
            else language_size = 0;
        }
        else if (language_size < lang_size + MAX_LANGUAGE_SIZE)
        {
            if (c >= 'a' && c <= 'z')
            {
                language_str[language_size - lang_size] = c;
                language_size++;
            }
            else
            {
                *language = language_get(language_str, language_size - lang_size);
                if (*language != '\0') return;
                language_size = (c == '?') ? 1 : 0;
            }
        }
        else
        {
            /* Do nothing */
        }
    }

    if (language_size >= lang_size)
    {
        *language = language_get(language_str, language_size - lang_size);
    }
}

bool processor_generic_paragraph(enum EntryStyle style)
{
    /*
    Space policy:
    ES_NORMAL, ES_LARGE, ES_LARGER, ES_LARGEST, ES_HEADER, ES_QUOTE are considered paragraphs
    HTTP: paragraphs use <p>, others are plaintext
    Others: maintain space before, after and between paragraphs
    */

    switch (style)
    {
    case ES_PARAGRAPH:
    case ES_QUOTE:
    case ES_LARGE:
    case ES_LARGER:
    case ES_LARGEST:
    case ES_HEADER:
        return true;
    default:
        return false;
    }
}

void processor_generic_upper_case(struct CharBuffer *string, size_t old_string_size, size_t prefix_size, size_t suffix_size)
{
    char *p;
    for (p = string->p + old_string_size + prefix_size; p < string->p + string->size - suffix_size; p++)
    {
        if (*p >= 'a' && *p <= 'z') *p -= (char)('a' + 'A');
    }
}

struct Error *processor_generic_box(struct CharBuffer *string, size_t old_string_size, size_t prefix_size, size_t suffix_size, char c1, char c2)
{
    const size_t full_line_size = string->size - old_string_size;
    const size_t line_size = full_line_size - prefix_size - suffix_size;
    PRET(string_resize(string, string->size + 2 * line_size));
    memcpy(string->p + old_string_size + full_line_size    , string->p + old_string_size, full_line_size);
    memcpy(string->p + old_string_size + full_line_size * 2, string->p + old_string_size, full_line_size);
    memset_alternating(string->p + old_string_size                      + prefix_size, c1, c2, line_size);
    memset_alternating(string->p + old_string_size + full_line_size * 2 + prefix_size, c1, c2, line_size);
    return OK;
}

static struct Error *processor_generic_contact(unsigned char a, struct ProcessorPrintContext *c) NODISCARD;
static struct Error *processor_generic_contact(unsigned char a, struct ProcessorPrintContext *c)
{
    const char *phone_str = (c->language == 'd') ? "Handy" : "Phone";
    if (ACCEPTING_SOCKET_IS_HTTP(a) || ACCEPTING_SOCKET_IS_HTTPS(a) || ACCEPTING_SOCKET_IS_GEMINI(a) || ACCEPTING_SOCKET_IS_SPARTAN(a) || ACCEPTING_SOCKET_IS_GUPPY(a))
    {
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "mailto:k.sovailo@gmail.com", "Email: k.sovailo@gmail.com"));
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "tel:+4917635479038", "%s: +49 1763 5479038", phone_str));
    }
    else
    {
        PRET(processor_print(a, c, ES_NORMAL, NULL, "Email: k.sovailo@gmail.com"));
        PRET(processor_print(a, c, ES_NORMAL, NULL, "%s: +49 1763 5479038", phone_str));
    }
    PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo", "Github"));
    PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://linkedin.com/in/kyrylo-sovailo-19b4541b9", "LinkedIn"));
    PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://www.xing.com/profile/Kyrylo_Sovailo", "Xing"));
    return OK;
}

static struct Error *processor_generic_protocols(unsigned char a, struct ProcessorPrintContext *c) NODISCARD;
static struct Error *processor_generic_protocols(unsigned char a, struct ProcessorPrintContext *c)
{
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "HTTP"));
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "HTTPS"));
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "Gopher"));
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "Finger"));
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "Gemini"));
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "Spartan"));
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "Nex"));
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "Text"));
    PRET(processor_print(a, c, ES_ITEMIZE, NULL, "Guppy"));
    return OK;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
static struct Error *processor_generic_footer(unsigned char a, struct ProcessorPrintContext *c, const char *resource) NODISCARD;
static struct Error *processor_generic_footer(unsigned char a, struct ProcessorPrintContext *c, const char *resource)
{
    const char *name;
    const char *names[7], *references[7];
    const char *footnote_str, *version_str, *why_str, *back_str, *updated_str;
    unsigned char i = 0;
    if (c->language == 'd')
    {
        footnote_str = "Footnote";
        version_str = "You are reading the %s version of this page. This page is also available through %s, %s, %s, %s, %s, %s, and %s:";
        why_str = "What are those and why can't I open them";
        back_str = "Back to the index page";
        updated_str = "Updated: 16 June 2026";
    }
    else
    {
        footnote_str = "Fußnote";
        version_str = "Sie lesen die %s-Version dieser Seite. Diese Seite ist auch über %s, %s, %s, %s, %s, %s, %s und %s verfügbar:";
        why_str = "Was sind diese Referenzen und warum kann ich sie nicht öffnen?";
        back_str = "Zurück zur Startseite";
        updated_str = "Aktualisiert: 16. Juni 2026";
    }

    PRET(processor_print(a, c, ES_LARGE, NULL, footnote_str));
    if (ACCEPTING_SOCKET_IS_HTTP(a))        name = "HTTP";
    else if (ACCEPTING_SOCKET_IS_HTTPS(a))  name = "HTTPS";
    else                                                             { names[i] = "HTTP";   references[i] = "http://"   DOMAIN_NAME HTTP_PORT_STRING;   i++; }
    if (ACCEPTING_SOCKET_IS_GOPHER(a))      name = "Gopher";    else { names[i] = "Gopher"; references[i] = "gopher://" DOMAIN_NAME GOPHER_PORT_STRING; i++; }
    if (ACCEPTING_SOCKET_IS_FINGER(a))      name = "Finger";    else { names[i] = "Finger"; references[i] = "finger://" DOMAIN_NAME FINGER_PORT_STRING; i++; }
    if (ACCEPTING_SOCKET_IS_GEMINI(a))      name = "Gemini";    else { names[i] = "Gemini"; references[i] = "gemini://" DOMAIN_NAME GEMINI_PORT_STRING; i++; }
    if (ACCEPTING_SOCKET_IS_SPARTAN(a))     name = "Spartan";   else { names[i] = "Spartan";references[i] = "spartan://"DOMAIN_NAME SPARTAN_PORT_STRING;i++; }
    if (ACCEPTING_SOCKET_IS_NEX(a))         name = "Nex";       else { names[i] = "Nex";    references[i] = "nex://"    DOMAIN_NAME NEX_PORT_STRING;    i++; }
    if (ACCEPTING_SOCKET_IS_TEXT(a))        name = "Text";      else { names[i] = "Text";   references[i] = "text://"   DOMAIN_NAME TEXT_PORT_STRING;   i++; }
    if (ACCEPTING_SOCKET_IS_GUPPY(a))       name = "Guppy";     else { names[i] = "Guppy";  references[i] = "guppy://"  DOMAIN_NAME GUPPY_PORT_STRING;  i++; }

    PRET(processor_print(a, c, ES_NORMAL, NULL, version_str,
        name, names[0], names[1], names[2], names[3], names[4], names[5], names[6]));
    for (i = 0; i < sizeof(names) / sizeof(*names); i++)
    {
        char reference[64];
        strcpy(reference, references[i]);
        if (resource != NULL) strcat(reference, resource);
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, reference, "%s", names[i]));
    }
    PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "smolnet", why_str));
    if (resource != NULL) { PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "", back_str)); }
    PRET(processor_print(a, c, ES_NORMAL, NULL, updated_str));
    return OK;
}
#pragma GCC diagnostic pop

struct Error *processor_generic(unsigned char a, struct ProcessorPrintContext *c, const struct ConstantValue *resource, bool *invalid_resource)
{
    struct ConstantValue normalized_resource = *resource;
    if (ACCEPTING_SOCKET_IS_HTTP(a) || ACCEPTING_SOCKET_IS_HTTPS(a) || ACCEPTING_SOCKET_IS_GEMINI(a) || ACCEPTING_SOCKET_IS_GUPPY(a))
        processor_generic_normalize_question(&normalized_resource, &c->language);
    else
        processor_generic_normalize_slash(&normalized_resource, &c->language);

    if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("")))
    {
        /* Index page */
        PRET(processor_print(a, c, ES_INITIALIZE, NULL, NULL));
        if (c->language == 'd')
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "Kyrylo's website"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "This is my website. There are many like it, but this one is mine."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Here you can learn more about me and about the server that powers this website:"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "about", "About me"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "twinkleshine", "About Twinkleshine"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "smolnet", "About SmolNet"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "resume", "My resume"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "projects", "My projects"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "contact", "My contacts"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Random fact: %s", fun_facts[random_rand(sizeof(fun_facts)/sizeof(fun_facts[0]))]));
        }
        else
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "Kyrylos Website"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Das hier ist meine Webseite. Es gibt viele andere, aber dies ist meins."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Hier könen Sie mehr über mich und über den Server, der diese Website betreibt, erfahren."));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "about", "Über mich"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "twinkleshine", "Über Twinkleshine"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "smolnet", "Über SmolNet"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "projects", "Meine Projekte"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "resume", "Mein Lebenslauf"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "contact", "Kontakt"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Zufälliger Fakt: %s", fun_facts_de[random_rand(sizeof(fun_facts_de)/sizeof(fun_facts_de[0]))]));
        }
        PRET(processor_generic_footer(a, c, NULL));
        PRET(processor_print(a, c, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("about")))
    {
        /* About me */
        PRET(processor_print(a, c, ES_INITIALIZE, NULL, NULL));
        if (c->language == 'd')
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "Über mich"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Mein Name ist Kyrylo."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Ich habe vor Kurzem mein Masterstudium in Computational Engineering an der TU Darmstadt abgeschlossen."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Ich mag Programmierung, Elektronik und Ponys."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Falls Ihr Unternehmen mit einer von drei etwas zu tun hat (idealerweise mit allen dreien) und Sie mich einstellen möchten (bitte tun Sie es), hier ist mein Lebenslauf zu finden:"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "resume", "Resume"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Einige meiner anderen Projekte sind hier aufgelistet:"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "projects", "Projekte"));
        }
        else
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "About me"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "My name is Kyrylo."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "I recently graduated from TU Darmstadt with a MSc in Computational Engineering."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "I like programming, electronics, and ponies."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "If your company has any of those (better all three) and you want to hire me (please do), here is my resume:"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "resume", "Resume"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Some of my other projects are listed here:"));
            PRET(processor_print(a, c, ES_INTERNAL_REFERENCE, "projects", "Projects"));
        }
        PRET(processor_generic_footer(a, c, "/about"));
        PRET(processor_print(a, c, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("twinkleshine")))
    {
        /* About twinkleshine */
        char time_since_startup[64];
        time_to_string_uptime(time_since_startup, c->language);
        PRET(processor_print(a, c, ES_INITIALIZE, NULL, NULL));
        if (c->language == 'd')
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "Twinkleshine"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Diese Website wird vom Twinkleshine-Server betrieben."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Twinkleshine wurde vollständig von mir in C 89 geschrieben. Sein charakteristisches Merkmal (abgesehen davon, dass er im Jahre des Herrn 2026 in C geschrieben wurde) ist, dass er nicht nur HTTP(S) unterstützt, sondern auch Gopher, Gemini und andere SmolNet-Protokolle. Alle Protokolle werden in einem Prozess bedient, die Seiten für verschiedene Protokolle werden mit derselben Logik generiert."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/Twinkleshine", "Quellcode"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Twinkleshine wurde hauptsächlich für Bildungs- und Demonstrationszwecke entwickelt. Die aktuelle Zeilenanzahl liegt unter 8000, also 30-mal weniger als bei nginx. Und weniger Zeilen bedeuten auch weniger Bugs. Theoretisch."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Das Projekt wurde nach Twinkleshine, dem Pony aus Camelot, benannt."));
            PRET(processor_print(a, c, ES_LARGE, NULL, "Unterstützte Protokolle"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Aktuell werden die folgenden Protokolle unterstützt:"));
            PRET(processor_generic_protocols(a, c));
            PRET(processor_print(a, c, ES_LARGE, NULL, "Zukünftige Pläne"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Da ich bereits alle von Lagrange unterstützten und die meisten von Mozz unterstützten Protokolle implementiert habe, ist die Phase der schnellen Erweiterung abgeschlossen. \"ftp://\", \"titan://\" und \"scorpion://\" bleiben die vielversprechendsten Kandidaten für Implementierung. Obwohl \"scroll://\" von Mozz unterstützt wird, scheint die Spezifikation des Protokolls nicht mehr zu finden sein, und ich bin nicht der Richtige, um \"scroll://\" zu retten. Es gibt Hunderte von SmolNet-Protokollen, und irgendwann musste ich aufhören."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Einige Abspaltungen von Twinkleshine können als Spielserver verwendet werden."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Twinkleshine könnte eines Tages auch ein eigenes Betriebssystem werden, das Projekt ist auch als Moondancer bekannt. In der Betriebssystementwicklung ist die Netzwerkkarte eines der einfachsten Hardwarestücke zu implementieren, daher ist es gar nicht so abwegig, wie es klingt."));
            PRET(processor_print(a, c, ES_LARGE, NULL, "Betriebszeit"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Wenn Sie dies lesen, bedeutet das, dass Twinkleshine es geschafft hat, binnen einer geraumen Zeit nicht abzustürzen."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Aktuelle Betriebszeit: %s.", time_since_startup));
        }
        else
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "Twinkleshine"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "This website is powered by the Twinkleshine server."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Twinkleshine is fully written by me in C 89. Its defining feature (apart from being written in C in the year of our Lord 2026) is that it supports not only HTTP(S), but also Gopher, Gemini, and other SmolNet protocols. All protocols are served by one process, pages for different protocols are generated using the exact same logic."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/Twinkleshine", "Source code"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Twinkleshine was developed mostly for educational and demonstrational purposes. Its current line count is under 8000, 30 times less than nginx. And less lines means less bugs. Theoretically."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "The project was named after Twinkleshine the pony from Camelot."));
            PRET(processor_print(a, c, ES_LARGE, NULL, "Supported protocols"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Currently, the following protocols are supported:"));
            PRET(processor_generic_protocols(a, c));
            PRET(processor_print(a, c, ES_LARGE, NULL, "Future plans"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Considering that I already implemented all protocols supported by Lagrange and most supported by Mozz, the phase of rapid expansion is over. \"ftp://\", \"titan://\" and \"scorpion://\" remain the most promising candidates for future implementation. Despite the fact that \"scroll://\" is supported by Mozz, the specification of the protocol seems to be lost to time, and I am not the right person to save \"scroll://\". There are hundreds of SmolNet protocols, and at some point I need to stop."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Some derivatives of Twinkleshine may be used to serve as game servers."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Twinkleshine may also one day become its own operating system, otherwise known as Project Moondancer. Network cards are one of the easiest hardware to implement in OS development, so it is not as crazy as it sounds."));
            PRET(processor_print(a, c, ES_LARGE, NULL, "Uptime"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "If you are reading this, it means that Twinkleshine managed not to fail for quite some time."));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Current uptime: %s.", time_since_startup));
        }
        PRET(processor_generic_footer(a, c, "/twinkleshine"));
        PRET(processor_print(a, c, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("smolnet")))
    {
        /* About SmolNet */
        const char *mozz_reference;
        if (ACCEPTING_SOCKET_IS_GOPHER(a)) mozz_reference = "gopher://mozz.us";
        else if (ACCEPTING_SOCKET_IS_GEMINI(a)) mozz_reference = "gemini://mozz.us";
        else if (ACCEPTING_SOCKET_IS_SPARTAN(a)) mozz_reference = "spartan://mozz.us";
        else mozz_reference = "https://mozz.us";
        PRET(processor_print(a, c, ES_INITIALIZE, NULL, NULL));
        PRET(processor_print(a, c, ES_HEADER, NULL, "SmolNet"));
        PRET(processor_print(a, c, ES_LARGE,  NULL, "Preamble"));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Long long ago, when dinosaurs roamed the Earth, all pages in the Internet looked like this: plain text on screens. Then Javascript ruined everything."));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "What most people know as \"the Internet\" is HTML pages delivered via the HTTP protocol. Remember how we used to type \"http://www\" before website names? Yes, that was protocol specification back when browsers didn't insert one automatically. HTML/HTTP eventually won over because it was flexible and allowed pictures. But it wasn't the first, nor was it, in fact, the last."));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Other protocols, like Gopher and Gemini, can be used to access information too. They, along with some others, are collectively known as \"SmolNet\", \"Small Net\", or, sometimes,  \"Small Web\"."));
        PRET(processor_print(a, c, ES_LARGE,  NULL, "Why can't I open the links?"));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Those are not HTTP(S) links. If you can't open them, it most probably means that your browser doesn't support them. Your browser can't \"speak\" the right language. Unfortunately, this is the case for the most popular browsers. Firefox once supported Gopher, but those days are gone."));
        PRET(processor_print(a, c, ES_LARGE,  NULL, "What makes SmolNet different?"));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Most notably, you won't see a lot of pictures or videos on the SmolNet. Why? For the same reason you don't see pictures in the books. It is distracting. SmolNet, and Gemini in particular, positions itself as a digital library."));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "HTTP/HTML allowed (and was inevitably overrun by) tracking, spyware, pop-up advertisements, and endless scroll. SmolNet protocols don't do that, can't do that, and some were developed to explicitly not have the capacity to do that. Not to say that commerce is not possible on SmolNet in principle, but in practice you won't find it."));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Furthermore, some people find it problematic that HTML pages demand you to display them as-is (e.g. in the exact font, color, and spacing, and with advertisements). They are, therefore, taking away your freedom."));
        PRET(processor_print(a, c, ES_LARGE,  NULL, "How can I access SmolNet?"));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, " SmolNet pages can be opened in special browsers, including:"));
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, ACCEPTING_SOCKET_IS_GEMINI(a) ? "gemini://skyjake.fi/lagrange" : "https://gmi.skyjake.fi/lagrange", "Lagrange (my recommendation)"));
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://lynx.invisible-island.net/", "Lynx (has HTTP support)"));
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://sr.ht/~julienxx/Castor", "Castor"));
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://kristall.random-projects.net", "Kristall"));
        PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "In case you aren't ready to install whole additional browser to access SmolNet, here are some alterative options:"));
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, mozz_reference, "Mozz.us (proxy)"));
        PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://addons.mozilla.org/de/firefox/addon/geminize", "Geminize (Firefox extension)"));
        PRET(processor_generic_footer(a, c, "/smolnet"));
        PRET(processor_print(a, c, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("contact")))
    {
        /* Contact information */
        PRET(processor_print(a, c, ES_INITIALIZE, NULL, NULL));
        if (c->language == 'd')
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "Contact information"));
        }
        else
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "Kontakt"));
        }
        PRET(processor_print(a, c, ES_NORMAL, NULL, "Kyrylo Sovailo."));
        PRET(processor_generic_contact(a, c));
        PRET(processor_generic_footer(a, c, "/contact"));
        PRET(processor_print(a, c, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("projects")))
    {
        /* My projects */
        PRET(processor_print(a, c, ES_INITIALIZE, NULL, NULL));

        if (c->language == 'd')
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "Meine Projekte"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Ein peinlich großer Teil meines Codes ist proprietär, zu spezifisch, verloren oder nicht der Rede wert. Trotzdem kann ich einigen Code teilen:"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Twinkleshine"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Der Server, der Ihnen diese Webseite gerade versandt hat."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/Twinkleshine", "Twinkleshine"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Kyrylos persönlicher Dispatcher"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Hilfreiches Tool zur Verwaltung von TODO.md. Besser als alles andere, was Sie finden können. Versprochen."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/PersonalDispatcher", "KPD"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Benchmark"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Vergleich der Leistung verschiedener Programmiersprachen - der fairste von allen. Die gewählte Aufgabe besteht darin, den kürzesten Pfad in einer dünnbesetzten Matrix zu finden. Spoiler: Assembler hat gewonnen."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/benchmark", "Benchmark"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Devtemplate"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Es mag zwar nichts tun, aber sehen Sie nur, wie gut es das tut! Mit Ikonchen, Handbüchern, Paketen und Installationsprogrammen!"));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/devtemplate", "Devtemplate"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "P6"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Simulation von Fachwerken. Ziemlich alt, hat baer eine schöne Benutzeroberfläche."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/P6", "P6"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Fool Moon"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Berechnet, ob Merkur rückläufig ist, mithilfe der unnötig komplexen Bahnmechanik."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/fool-moon", "Fool Moon"));
        }
        else
        {
            PRET(processor_print(a, c, ES_HEADER, NULL, "My projects"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Embarrassingly large part of my code is proprietary, oddly specific, lost to time, or not worthy of display. Still, here is something I can share:"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Twinkleshine"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "The server sending you this text right now."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/Twinkleshine", "Twinkleshine"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Kyrylo's Personal Dispatcher"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Helpful utility to manage TODO.md. Better than everything you'll find, I promise."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/PersonalDispatcher", "KPD"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Benchmark"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Comparison of the programming language performances, fairest of them all. The selected problem is finding the shortest path in a sparse matrix. Spoiler: assembly won."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/benchmark", "Benchmark"));

            PRET(processor_print(a, c, ES_LARGE, NULL, "Devtemplate"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "It may do nothing, but look how well it does it! With icons and manuals and packages and installers!"));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/devtemplate", "Devtemplate"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "P6"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Simulation of truss networks. Quite old, but nice GUI."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/P6", "P6"));
            
            PRET(processor_print(a, c, ES_LARGE, NULL, "Fool Moon"));
            PRET(processor_print(a, c, ES_PARAGRAPH, NULL, "Calculates whether or not Mercury is in retrograde by running an unnecessarily complex orbital mechanics."));
            PRET(processor_print(a, c, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/fool-moon", "Fool Moon"));
        }
        
        PRET(processor_generic_footer(a, c, "/projects"));
        PRET(processor_print(a, c, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("resume")))
    {
        /* Resume */
        PRET(processor_print(a, c, ES_INITIALIZE, NULL, NULL));
        PRET(processor_print(a, c, ES_HEADER, NULL, "Kyrylo Sovailo"));
        PRET(processor_generic_contact(a, c));
        if (c->language == 'd')
        {
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Date of Birth: 08.10.2000"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Education"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Computational Engineering (CE) M.Sc."));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "At TU Darmstadt"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Specialization: Computational Robotics"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Duration: 04.2023 - 12.2025"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Final Grade: 2.1"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Thesis Grade: 1.7"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Thesis Topic: Adding spatial awareness to embedding-based anomaly detection methods for automated industrial inspection"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Thesis result: Developed an anomaly detection method with 95%% accuracy by combining classical algorithms with foundation models (Python, PyTorch, Transformers)."));

            PRET(processor_print(a, c, ES_LARGE,  NULL, "Computational Engineering Science (CES) B.Sc."));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "At RWTH Aachen University"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Duration: 10.2018 - 04.2023"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Final Grade: 2.4"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Thesis Grade: 1.7"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Thesis Topic: Balancing a 2D inverted pendulum with a Franka Panda robotic arm"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Thesis result: Developed inverse kinematics, inverse dynamics, and control algorithms that enabled the robot to balance a 2-DOF inverse pendulum (ROS, C++, Python)."));

            PRET(processor_print(a, c, ES_LARGE,  NULL, "High School"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "At Studienkolleg at Free University Berlin, Technical Course"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Duration: 09.2017 - 09.2018"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Professional Experience"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Student Assistant"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "At Goethe University Frankfurt (Buchmann Institute for Molecular Life Sciences, BMLS)"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Duration: 06.2023 - 07.2024"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Key achievements:"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Developed graphical slicing software with support for features unique to a bio-3D-printer (C#, C++, Slic3r)."));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Ported printer control software to new hardware (C++, Arduino)."));

            PRET(processor_print(a, c, ES_LARGE,  NULL, "Intern"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "At Continental AG"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Duration: 03.2022 - 05.2022"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Key achievements:"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Developed a tool for generation of C code from CAN database (Python, C, CAPL)."));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Automated the software build process for a V850 microcontroller (CMake, Python)."));
            
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Student Assistant"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "At RWTH Aachen University (Institute for Data Science in Mechanical Engineering, DSME)"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Duration: 04.2021 - 12.2021"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Key achievements:"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Implemented high-level APIs and developed control algorithms for a Franka Panda robotic arm (real-time Linux, C++, Python)."));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Implemented a standalone (non-ROS) software simulator for the robot (C++, Gazebo)."));

            PRET(processor_print(a, c, ES_LARGER, NULL, "IT Skills"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Software Development"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Expert: C, C++, Python, CMake"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Advanced: Matlab/Simulink, PyTorch, Git, Linux, LaTeX"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Hardware Development"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Advanced: KiCAD, FreeCAD"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Basics: Autodesk Inventor"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Embedded Development"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Expert: C/C++ (Atmel AVR, ARM, Raspberry Pi, Nvidia Jetson, x86_64 real-time Linux), Python (Nvidia Jetson, x86_64 real-time Linux)"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Advanced: ROS, stationary robots (Franka Panda), mobile robots (Turtlebot, Jetbot)"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Basics: FPGA programming"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Technical Skills"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Advanced: Soldering, PCB debugging"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Languages"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Native: Ukrainian, Russian"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Fluent (C1): English, German"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Awards"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Bronze Medal, International Physics Olympiad (IPhO), 2017"));
        }
        else
        {
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Geboren am: 08.10.2000"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Ausbildung"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Computational Engineering (CE) M.Sc."));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "An der TU Darmstadt"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Vertiefung: Computational Robotics"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Zeitraum: 04.2023 - 12.2025"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Abschlussnote: 2.1"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Note der Abschlussarbeit: 1.7"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Thema der Abschlussarbeit: Integration von räumlicher Wahrnehmung in embeddings-basierte Anomalieerkennungsmethoden für die automatisierte industrielle Inspektion"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Ergebnis der Abschlussarbeit: Anomalieerkennungsmethode mit einer Genauigkeit von 95%% durch Kombination klassischer Algorithmen mit Foundation Models entwickelt (Python, PyTorch, Transformers)."));

            PRET(processor_print(a, c, ES_LARGE,  NULL, "Computational Engineering Science (CES) B.Sc."));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "An der RWTH Aachen University"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Zeitraum: 10.2018 - 04.2023"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Final Grade: 2.4"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Note der Abschlussarbeit: 1.7"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Thema der Abschlussarbeit: Balancierung eines 2D inversen Pendels mit Franka Panda Roboterarm"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Ergebnis der Abschlussarbeit: Steuerungsalgorithmen mit inverser Kinematik und Dynamik entwickelt, die dem Roboter ermöglichten, ein inverses Pendel mit zwei Freiheitsgraden zu balancieren (ROS, C++, Python)."));

            PRET(processor_print(a, c, ES_LARGE,  NULL, "Abitur"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Studienkolleg an Freier Universität Berlin"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Zeitraum: 09.2017 - 09.2018"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Berufspraktische Erfahrungen"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Studentische Hilfskraft"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "An der Goethe Universität Frankfurt (Buchmann Institute for Molecular Life Sciences, BMLS)"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Zeitraum: 06.2023 - 07.2024"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Wichtigste Ergebnise:"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Grafische Slicing-Software entwickelt und die für einen Bio-3D-Drucker spezifischen Funktionen implementiert (C#, C++, Slic3r)."));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Software für die Druckersteuerung auf die neue Hardware portiert (C++, Arduino)."));

            PRET(processor_print(a, c, ES_LARGE,  NULL, "Praktikant"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Bei Continental AG"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Zeitraum: 03.2022 - 05.2022"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Wichtigste Ergebnise:"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Tool zur Generierung von C-Quellcode aus der CAN-Datenbank entwickelt (Python, C, CAPL)."));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Build-Prozess der Software für einen V850-Mikrocontroller automatisiert (CMake, Python)."));
            
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Studentische Hilfskraft"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "An der RWTH Aachen University (Institute for Data Science in Mechanical Engineering, DSME)"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Zeitraum: 04.2021 - 12.2021"));
            PRET(processor_print(a, c, ES_NORMAL, NULL, "Wichtigste Ergebnise:"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "High-Level-APIs für einen Franka-Panda-Roboterarm implementiert und Steuerungsalgorithmen entwickelt (Echtzeit-Linux, C++, Python)."));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Eigenständige (ohne ROS) Simulationssoftware für den Arm entwickelt (C++, Gazebo)."));

            PRET(processor_print(a, c, ES_LARGER, NULL, "IT-Kenntnisse"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Softwareentwicklung"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Experte: C, C++, Python, CMake"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Fortgeschrittener: Matlab/Simulink, PyTorch, Git, Linux, LaTeX"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Hardwareentwicklung"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Fortgeschrittener: KiCAD, FreeCAD"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Grundkenntnisse: Autodesk Inventor"));
            PRET(processor_print(a, c, ES_LARGE,  NULL, "Embedded-Entwicklung"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Experte: C/C++ (Atmel AVR, ARM, Raspberry Pi, Nvidia Jetson, x86_64 Echtzeit-Linux), Python (Nvidia Jetson, x86_64 Echtzeit-Linux)"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Fortgeschrittener: ROS, stationäre Roboter (Franka Panda), mobile Roboter (Turtlebot, Jetbot)"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Grundkenntnisse: FPGA-Programming"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Technische Kenntnisse"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Fortgeschrittener: Löten, Leiterplatten-Debugging"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Sprachen"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Muttersprache: Ukrainisch, Russisch"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Fließend (C1): Englisch, Deutsch"));

            PRET(processor_print(a, c, ES_LARGER, NULL, "Auszeichnungen"));
            PRET(processor_print(a, c, ES_ITEMIZE,NULL, "Bronzemedaille bei der Internationalen Physikolympiade (IPhO), 2017"));
        }
        PRET(processor_generic_footer(a, c, "/resume"));
        PRET(processor_print(a, c, ES_FINALIZE, NULL, NULL));
    }
    else *invalid_resource = true;
    return OK;
}