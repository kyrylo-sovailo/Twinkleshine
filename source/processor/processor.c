#include "../../include/processor.h"
#include "../../commonlib/include/string.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/random.h"
#include "../../include/tables.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

struct CharBuffer g_internal_buffer_one = ZERO_INIT; /* TODO: make it static */
struct CharBuffer g_internal_buffer_two = ZERO_INIT;
static time_t g_startup_time;

static void print_time_since_startup(char buffer[64])
{
    time_t now, difference;
    unsigned int days, hours, minutes, seconds;
    const char *days_str = "days", *hours_str = "hours", *minutes_str = "minutes", *seconds_str = "seconds";
    unsigned char i = 4;
    
    now = time(NULL);
    if (now <= g_startup_time) { strcpy(buffer, "0 seconds"); return; }
    difference = now - g_startup_time;
    days = (unsigned int)(difference / DAY);
    hours = (unsigned int)(difference / HOUR) % 24;
    minutes = (unsigned int)(difference / MINUTE) % 60;
    seconds = (unsigned int)(difference / SECOND) % 60;

    if (days == 0) i--; else if (days == 1) days_str = "day";
    if (hours == 0) i--; else if (hours == 1) hours_str = "hour";
    if (minutes == 0) i--; else if (minutes == 1) minutes_str = "minute";
    /* if (seconds == 0) i--; else */ if (seconds == 1) seconds_str = "second";
    
    if (i == 4) sprintf(buffer, "%u %s, %u %s, %u %s, and %u %s", days, days_str, hours, hours_str, minutes, minutes_str, seconds, seconds_str);
    else if (i == 3) sprintf(buffer, "%u %s, %u %s, and %u %s", hours, hours_str, minutes, minutes_str, seconds, seconds_str);
    else if (i == 2) sprintf(buffer, "%u %s and %u %s", minutes, minutes_str, seconds, seconds_str);
    else /* if (i == 1) */ sprintf(buffer, "%u %s", seconds, seconds_str);
}

static struct Error *processor_print(int type, struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, ...) PRINTFLIKE(5, 6) NODISCARD;
static struct Error *processor_print(int type, struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, ...)
{
    struct Error *error;
    va_list va;
    va_start(va, format);
    switch (type)
    {
    case CT_HTTP:   PGOTO(processor_print_http(context, style, resource, format, va));      break;
    case CT_GOPHER: PGOTO(processor_print_gopher(context, style, resource, format, va));    break;
    case CT_FINGER: PGOTO(processor_print_finger(context, style, resource, format, va));    break;
    default:        PGOTO(processor_print_gemini(context, style, resource, format, va));    break; /* CT_GEMINI */
    }
    error = OK;
    failure:
    va_end(va);
    return error;
}

static struct Error *processor_process_generic_footer(int type, struct ProcessorPrintContext *context, const char *resource) NODISCARD;
static struct Error *processor_process_generic_footer(int type, struct ProcessorPrintContext *context, const char *resource)
{
    const char *name;
    const char *names[3], *references[3];
    unsigned char i = 0;
    PRET(processor_print(type, context, ES_LARGE, NULL, "Footnote"));
    if (type == CT_HTTP)   name = "HTTP";   else { names[i] = "HTTP";   references[i] = "http://"   DOMAIN_NAME HTTP_PORT_STRING;   i++; }
    if (type == CT_GOPHER) name = "Gopher"; else { names[i] = "Gopher"; references[i] = "gopher://" DOMAIN_NAME GOPHER_PORT_STRING; i++; }
    if (type == CT_FINGER) name = "Finger"; else { names[i] = "Finger"; references[i] = "finger://" DOMAIN_NAME FINGER_PORT_STRING; i++; }
    if (type == CT_GEMINI) name = "Gemini"; else { names[i] = "Gemini"; references[i] = "gemini://" DOMAIN_NAME GEMINI_PORT_STRING; i++; }
    PRET(processor_print(type, context, ES_NORMAL, NULL, "You are reading the %s version of this page. This page is also available through %s, %s and %s:", name, names[0], names[1], names[2]));
    for (i = 0; i < 3; i++)
    {
        char reference[32];
        strcpy(reference, references[i]);
        if (resource != NULL) strcat(reference, resource);
        PRET(processor_print(type, context, ES_EXTERNAL_REFERENCE, reference, "%s", names[i]));
    }
    PRET(processor_print(type, context, ES_INTERNAL_REFERENCE, "smallnet", "What are those and why can't I open them"));
    if (resource != NULL) { PRET(processor_print(type, context, ES_INTERNAL_REFERENCE, "", "Back to the index page")); }
    PRET(processor_print(type, context, ES_NORMAL, NULL, "Updated: 4 May 2026"));
    return OK;
}

static struct Error *processor_process_generic(int type, const struct Value *resource, const struct Request *request, bool *invalid_resource) NODISCARD;
static struct Error *processor_process_generic(int type, const struct Value *resource, const struct Request *request, bool *invalid_resource)
{
    struct ProcessorPrintContext context = { &g_internal_buffer_one, &g_internal_buffer_two, request, 0, 0 };
    struct Value normalized_resource = *resource;
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        if (normalized_resource.parts[i].size > 0 && *normalized_resource.parts[i].p == '/') { normalized_resource.parts[i].p++; normalized_resource.parts[i].size--; break; }
    }

    if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("")))
    {
        /* Index page */
        PRET(processor_print(type, &context, ES_INITIALIZE, NULL, NULL));
        PRET(processor_print(type, &context, ES_HEADER, NULL, "Kyrylo's website"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "This is my website. There are many like it, but this one is mine."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Here you can learn more about me and about the server that powers this website:"));
        PRET(processor_print(type, &context, ES_INTERNAL_REFERENCE, "about", "About me"));
        PRET(processor_print(type, &context, ES_INTERNAL_REFERENCE, "twinkleshine", "About Twinkleshine"));
        PRET(processor_print(type, &context, ES_INTERNAL_REFERENCE, "smallnet", "About smallnet"));
        PRET(processor_print(type, &context, ES_INTERNAL_REFERENCE, "resume", "My professional resume"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Random fact: %s", fun_facts[random_rand(sizeof(fun_facts)/sizeof(fun_facts[0]))]));
        PRET(processor_process_generic_footer(type, &context, NULL));
        PRET(processor_print(type, &context, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("about")))
    {
        /* About me */
        PRET(processor_print(type, &context, ES_INITIALIZE, NULL, NULL));
        PRET(processor_print(type, &context, ES_HEADER, NULL, "About me"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "My name is Kyrylo."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "I recently graduated from TU Darmstadt with a MSc in Computational Engineering."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "I like programming, electronics, and ponies."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "If your company has any of those (better all three) and you want to hire me (please do), here is my resume:"));
        PRET(processor_print(type, &context, ES_INTERNAL_REFERENCE, "resume", "Resume"));
        PRET(processor_process_generic_footer(type, &context, "/about"));
        PRET(processor_print(type, &context, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("twinkleshine")))
    {
        /* About twinkleshine */
        char time_since_startup[64];
        print_time_since_startup(time_since_startup);
        PRET(processor_print(type, &context, ES_INITIALIZE, NULL, NULL));
        PRET(processor_print(type, &context, ES_HEADER, NULL, "Twinkleshine"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "This website is powered by the Twinkleshine server."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Twinkleshine is fully written by me in C 89. Its defining feature (apart from being written in C in the year of our Lord 2026) is that it serves as HTTP(S), Gopher, Finger, and Gemini server, all in one process. Pages for different protocols are generated using the exact same logic."));
        PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo/Twinkleshine", "Source code"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Twinkleshine was developed mostly for educational and demonstrational purposes. Its current line count is under 8000, 30 times less than nginx. And less lines means less bugs. Theoretically."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Another potential application of Twinkleshine would be to delegate it network handling and to run some custom modules. And do so efficiently, since no inter-process communication is taking place."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "The project was named after Twinkleshine the pony from Camelot."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "If you are reading this, it means that Twinkleshine managed not to fail for quite some time."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Current uptime: %s.", time_since_startup));
        PRET(processor_process_generic_footer(type, &context, "/twinkleshine"));
        PRET(processor_print(type, &context, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("smallnet")))
    {
        /* About smallnet */
        PRET(processor_print(type, &context, ES_INITIALIZE, NULL, NULL));
        PRET(processor_print(type, &context, ES_HEADER, NULL, "Smallnet"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Long long ago, when dinosaurs roamed the Earth, all pages in the Internet looked like this: plain text on screens. Then Javascript ruined everything."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "What most people know as 'the Internet' is HTML pages delivered by HTTP protocol. Remember how we used to write 'https://www' before website names? Yes, that's it. HTML/HTTP became popular because it was flexible (and allowed pictures). But it wasn't the first, nor was it, in fact, the last."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Other protocols (Gopher, Finger, Gemini) can be used to access information too. They are collectively known as 'smallnet'. And this is how smallnet looks like - it's mostly text."));
        PRET(processor_print(type, &context, ES_LARGE,  NULL, "But why?"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Why would anyone want to reject pictures?"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "For the same reason you don't see pictures in the books and don't hear music in the library. It is distracting."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "HTTP(S) allowed (and was inevitably overrun by) tracking, spyware, pop-up advertisements, and endless scroll."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Furthermore, some people find it problematic that HTTP(S) pages demand you to display them as-is (e.g. with advertisements) and run arbitrary pieces of code. They are, therefore, taking away your freedom. Smallnet protocols don't do that, can't do that, and some were developed to not have the capacity to do that."));
        PRET(processor_print(type, &context, ES_LARGE,  NULL, "How can I join?"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Most popular browsers don't support smallnet protocols. Firefox once supported Gopher, not anymore. Smallnet pages can be opened in special browsers, including:"));
        PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, (type == CT_GEMINI) ? "gemini://skyjake.fi/lagrange" : "https://gmi.skyjake.fi/lagrange", "Lagrange (my recommendation)"));
        PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, "https://sr.ht/~julienxx/Castor", "Castor"));
        PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, "https://kristall.random-projects.net", "Kristall"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Be aware though that none (to my knowledge) smallnet browsers support HTTP(S). Here are some alterative options:"));
        PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, (type == CT_GOPHER) ? "gopher://mozz.us" : ((type == CT_GEMINI) ? "gemini://mozz.us" : "https://mozz.us"), "Mozz.us (proxy)"));
        PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, "https://addons.mozilla.org/de/firefox/addon/geminize", "Geminize (Firefox extension)"));
        PRET(processor_process_generic_footer(type, &context, "/smallnet"));
        PRET(processor_print(type, &context, ES_FINALIZE, NULL, NULL));
    }
    else if (value_compare_case_mem(&normalized_resource, STRING_STRLEN("resume")))
    {
        /* CV */
        PRET(processor_print(type, &context, ES_INITIALIZE, NULL, NULL));
        PRET(processor_print(type, &context, ES_HEADER, NULL, "Kyrylo Sovailo"));
        if (type == CT_HTTP)
        {
            PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, "mailto:k.sovailo@gmail.com", "Email: k.sovailo@gmail.com"));
            PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, "tel:+4917635479038", "Phone: +49 1763 5479038"));
        }
        else
        {
            PRET(processor_print(type, &context, ES_NORMAL, NULL, "Email: k.sovailo@gmail.com"));
            PRET(processor_print(type, &context, ES_NORMAL, NULL, "Phone: +49 1763 5479038"));
        }
        PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, "https://github.com/kyrylo-sovailo", "Github"));
        PRET(processor_print(type, &context, ES_EXTERNAL_REFERENCE, "https://linkedin.com/in/kyrylo-sovailo-19b4541b9", "LinkedIn"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Address: Pallaswiesenstr. 39, 64293 Darmstadt"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Date of Birth: 08.10.2000"));

        PRET(processor_print(type, &context, ES_LARGER, NULL, "Education"));
        PRET(processor_print(type, &context, ES_LARGE,  NULL, "Computational Engineering (CE) M.Sc."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "At TU Darmstadt"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Specialization: Computational Robotics"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Duration: 04.2023 - 12.2025"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Final Grade: 2.1"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Thesis Grade: 1.7"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Thesis Topic: Adding spatial awareness to embedding-based anomaly detection methods for automated industrial inspection"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Thesis result: Developed an anomaly detection method with 95%% accuracy by combining classical algorithms with foundation models (Python, PyTorch, Transformers)."));

        PRET(processor_print(type, &context, ES_LARGE,  NULL, "Computational Engineering Science (CES) B.Sc."));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "At RWTH Aachen University"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Duration: 10.2018 - 04.2023"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Final Grade: 2.4"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Thesis Grade: 1.7"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Thesis Topic: Balancing a 2D inverted pendulum with a Franka Panda robotic arm"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Thesis result: Developed inverse kinematics, inverse dynamics, and control algorithms that enabled the robot to balance a 2-DOF inverse pendulum (ROS, C++, Python)."));

        PRET(processor_print(type, &context, ES_LARGE,  NULL, "High School"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "At Studienkolleg at Free University Berlin, Technical Course"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Duration: 09.2017 - 09.2018"));

        PRET(processor_print(type, &context, ES_LARGER, NULL, "Professional Experience"));
        PRET(processor_print(type, &context, ES_LARGE,  NULL, "Student Assistant"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "At Goethe University Frankfurt (Buchmann Institute for Molecular Life Sciences, BMLS)"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Duration: 06.2023 - 07.2024"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Key achievements:"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Developed graphical slicing software with support for features unique to a bio-3D-printer (C#, C++, Slic3r)."));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Ported printer control software to new hardware (C++, Arduino)."));

        PRET(processor_print(type, &context, ES_LARGE,  NULL, "Intern"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "At Continental AG"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Duration: 03.2022 - 05.2022"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Key achievements:"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Developed a tool for generation of C code from CAN database (Python, C, CAPL)."));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Automated the software build process for a V850 microcontroller (CMake, Python)."));
        
        PRET(processor_print(type, &context, ES_LARGE,  NULL, "Student Assistant"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "At RWTH Aachen University (Institute for Data Science in Mechanical Engineering, DSME)"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Duration: 04.2021 - 12.2021"));
        PRET(processor_print(type, &context, ES_NORMAL, NULL, "Key achievements:"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Implemented high-level APIs and developed control algorithms for a Franka Panda robotic arm (real-time Linux, C++, Python)."));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Implemented a standalone (non-ROS) software simulator for the robot (C++, Gazebo)."));

        PRET(processor_print(type, &context, ES_LARGER, NULL, "IT Skills"));
        PRET(processor_print(type, &context, ES_LARGE,  NULL, "Software Development"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Expert: C, C++, Python, CMake"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Advanced: Matlab/Simulink, PyTorch, Git, Linux, LaTeX"));
        PRET(processor_print(type, &context, ES_LARGE,  NULL, "Hardware Development"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Advanced: KiCAD, FreeCAD"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Basics: Autodesk Inventor"));
        PRET(processor_print(type, &context, ES_LARGE,  NULL, "Embedded Development"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Expert: C/C++ (Atmel AVR, ARM, Raspberry Pi, Nvidia Jetson, x86_64 real-time Linux), Python (Nvidia Jetson, x86_64 real-time Linux)"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Advanced: ROS, stationary robots (Franka Panda), mobile robots (Turtlebot, Jetbot)"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Basics: FPGA programming"));

        PRET(processor_print(type, &context, ES_LARGER, NULL, "Technical Skills"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Advanced: Soldering, PCB debugging"));

        PRET(processor_print(type, &context, ES_LARGER, NULL, "Languages"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Native: Ukrainian, Russian"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Fluent (C1): English, German"));

        PRET(processor_print(type, &context, ES_LARGER, NULL, "Awards"));
        PRET(processor_print(type, &context, ES_ITEMIZE,NULL, "Bronze Medal, International Physics Olympiad (IPhO), 2017"));

        PRET(processor_process_generic_footer(type, &context, "/resume"));
        PRET(processor_print(type, &context, ES_FINALIZE, NULL, NULL));
    }
    else *invalid_resource = true;
    return OK;
}

static struct ExError processor_process_specific(int type, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
static struct ExError processor_process_specific(int type, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    switch (type)
    {
    case CT_HTTP:   EXPRET(processor_process_http(request, request_stream, response, response_queue, response_stream));     break;
    case CT_GOPHER: EXPRET(processor_process_gopher(request, request_stream, response, response_queue, response_stream));   break;
    case CT_FINGER: EXPRET(processor_process_finger(request, request_stream, response, response_queue, response_stream));   break;
    default:        EXPRET(processor_process_gemini(request, request_stream, response, response_queue, response_stream));   break; /* CT_GEMINI */
    }
    return EXOK;
}

struct Error *processor_module_initialize(void)
{
    PRET(processor_module_initialize_http());
    g_startup_time = time(NULL);
    return OK;
}

void processor_module_finalize(void)
{
    processor_module_initialize_http();
}

struct ExError processor_process(int type, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    struct Value method;

    EXPRETF(ring_get(request_stream, &request->method, false, &method), EEF_CLOSE_LOG_DIE);
    if (type != CT_HTTP || value_compare_case_mem(&method, STRING_STRLEN("get")))
    {
        /* Generic text printing */
        struct Value resource, protocol;
        bool invalid_resource = false;
        EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);
        EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol), EEF_CLOSE_LOG_DIE);
        EXPRETF(processor_process_generic(type, &resource, request, &invalid_resource), EEF_SEND_CLOSE_LOG(FR_UNKNOWN));
        if (!invalid_resource)
        {
            response_stream->parts[0].p = g_internal_buffer_one.p;
            response_stream->parts[0].size = g_internal_buffer_one.size;
            response_stream->parts[1].p = g_internal_buffer_two.p;
            response_stream->parts[1].size = g_internal_buffer_two.size;
            EXPRET(processor_construct_response(response, response_queue,
                value_const_size(response_stream), request->keep_alive, &method, &resource, &protocol));
            return EXOK;
        }
    }

    /* Specific response */
    EXPRET(processor_process_specific(type, request, request_stream, response, response_queue, response_stream));
    return EXOK;
}

struct ExError processor_fixed(int type, enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    switch (type)
    {
    case CT_HTTP:   EXPRET(processor_fixed_http(fixed, response, response_queue, response_stream));     break;
    case CT_GOPHER: EXPRET(processor_fixed_gopher(fixed, response, response_queue, response_stream));   break;
    case CT_FINGER: EXPRET(processor_fixed_finger(fixed, response, response_queue, response_stream));   break;
    default:        EXPRET(processor_fixed_gemini(fixed, response, response_queue, response_stream));   break; /* CT_GEMINI */
    }
    return EXOK;
}

void processor_fixed_failsafe(int type, enum FixedResponse fixed, struct ConstValue *response_stream)
{
    switch (type)
    {
    case CT_HTTP:   processor_fixed_http_failsafe(fixed, response_stream);      break;
    case CT_GOPHER: processor_fixed_gopher_failsafe(fixed, response_stream);    break;
    case CT_FINGER: processor_fixed_finger_failsafe(fixed, response_stream);    break;
    default:        processor_fixed_gemini_failsafe(fixed, response_stream);    break; /* CT_GEMINI */
    }
}

void processor_free(void)
{
    /* Do nothing */
}

struct ExError processor_construct_response(struct Response *response, struct Ring *response_queue,
    size_t stream_size, bool keep_alive,
    const struct Value *method, const struct Value *resource, const struct Value *protocol)
{
    const struct ExError EXOK = { OK };
    unsigned char i;

    response->stream_size = stream_size;
    response->keep_alive = keep_alive;
    response->phony = false;
    response->size = 0;

    response->method.offset = response->size;
    response->method.size = value_size(method);
    response->size += response->method.size;

    response->resource.offset = response->size;
    response->resource.size = value_size(resource);
    response->size += response->resource.size;

    response->protocol.offset = response->size;
    response->protocol.size = value_size(protocol);
    response->size += response->protocol.size;

    EXPRETF(ring_reserve(response_queue, response_queue->size + sizeof(struct Response) + response->size), EEF_SEND_SHUTDOWN_LOG(FR_UNKNOWN));
    EXPRETF(ring_push_write(response_queue, sizeof(struct Response), (const char*)response), PARTIAL_EEF_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(response_queue, method->parts[i].size, method->parts[i].p), PARTIAL_EEF_DIE);
    }
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(response_queue, resource->parts[i].size, resource->parts[i].p), PARTIAL_EEF_DIE);
    }
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(response_queue, protocol->parts[i].size, protocol->parts[i].p), PARTIAL_EEF_DIE);
    }
    return EXOK;
}
