/* SPDX-License-Identifier: MIT
 * GroomLab SDK sample (MIT licence, LICENSE.txt): free to copy into your own bridge.
 *
 * groom_counts: loads a .groom file with the plain C API, evaluates it and prints its strand and
 * point counts.
 *
 *   groom_counts hair.groom [frame]
 */
#include <stdio.h>
#include <stdlib.h>

#include "groomlab/groomlab_c.h"

int main(int argc, char** argv) {
    gl_session* session = NULL;
    gl_strands* strands = NULL;
    gl_result result;

    if (argc < 2) {
        fprintf(stderr, "usage: groom_counts file.groom [frame]\n");
        return 2;
    }
    if (!GROOMLAB_C_API_COMPATIBLE(gl_api_version())) {
        fprintf(stderr, "groom_counts: GroomLab %s does not serve C API %d.%d\n", gl_version_string(),
                GROOMLAB_C_API_VERSION_MAJOR, GROOMLAB_C_API_VERSION_MINOR);
        return 1;
    }

    result = gl_session_create(&session);
    if (result == GL_OK) result = gl_session_load_file(session, argv[1]);
    if (result == GL_OK && argc > 2) result = gl_session_set_frame(session, atof(argv[2]));
    if (result == GL_OK) result = gl_session_evaluate(session, NULL, &strands);
    if (result != GL_OK) {
        /* Read the message before the next gl_* call replaces it. */
        fprintf(stderr, "groom_counts: %s: %s\n", gl_result_name(result), gl_last_error());
        gl_session_destroy(session);
        return 1;
    }

    printf("%zu strands, %zu points\n", gl_strands_strand_count(strands), gl_strands_point_count(strands));
    gl_strands_release(strands); /* the result outlives the session; release both */
    gl_session_destroy(session);
    return 0;
}
