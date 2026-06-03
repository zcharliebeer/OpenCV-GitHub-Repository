/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.
//
//M*/

#include "precomp.hpp"
#include "grfmt_jpeg.hpp"

#ifdef HAVE_JPEG

#include <iostream>
#include <setjmp.h>

// Loop filter offset table
static const int  LFO_00_07 = 0;
static const int  LFO_08_15 = 8;
static const int  LFO_16_23 = 16;
static const int  LFO_24_31 = 24;
static const int  LFO_32_39 = 32;
static const int  LFO_40_47 = 40;
static const int  LFO_48_55 = 48;
static const int  LFO_56_63 = 56;

namespace cv
{

struct my_error_mgr
{
    struct jpeg_error_mgr pub;  /* "public" fields */
    jmp_buf setjmp_buffer;      /* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

struct JpegState
{
    jpeg_decompress_struct cinfo;
    my_error_mgr jerr;
    bool active;
};

JpegDecoder::JpegDecoder()
{
    m_signature = "\xFF\xD8\xFF";
    m_state = 0;
    m_f = 0;
}

JpegDecoder::~JpegDecoder()
{
    close();
}

ImageDecoder JpegDecoder::newDecoder() const
{
    return makePtr<JpegDecoder>();
}

void JpegDecoder::close()
{
    if( m_f )
    {
        fclose( m_f );
        m_f = 0;
    }

    if( m_state )
    {
        JpegState* state = (JpegState*)m_state;
        if( state->active )
        {
            if( setjmp( state->jerr.setjmp_buffer ) == 0 )
            {
                jpeg_destroy_decompress( &state->cinfo );
            }
            state->active = false;
        }
        delete state;
        m_state = 0;
    }
}

bool JpegDecoder::readHeader()
{
    bool result = false;
    close();

    JpegState* state = new JpegState;
    m_state = state;
    state->active = false;

    m_f = fopen( m_filename.c_str(), "rb" );
    if( !m_f )
        return false;

    state->cinfo.err = jpeg_std_error( &state->jerr.pub );
    state->jerr.pub.error_exit = my_error_exit;

    if( setjmp( state->jerr.setjmp_buffer ) )
    {
        close();
        return false;
    }

    jpeg_create_decompress( &state->cinfo );
    state->active = true;
    jpeg_stdio_src( &state->cinfo, m_f );
    jpeg_read_header( &state->cinfo, TRUE );

    m_width = state->cinfo.image_width;
    m_height = state->cinfo.image_height;
    m_type = state->cinfo.num_components == 1 ? CV_8UC1 : CV_8UC3;
    result = true;

    return result;
}

bool JpegDecoder::readData( Mat& img )
{
    bool result = false;
    uchar* data = img.data;
    int step = (int)img.step;
    bool color = img.channels() > 1;

    if( !m_state )
        return false;

    JpegState* state = (JpegState*)m_state;

    if( setjmp( state->jerr.setjmp_buffer ) )
    {
        close();
        return false;
    }

    jpeg_start_decompress( &state->cinfo );

    if( state->cinfo.num_components == 1 && color )
    {
        // grayscale -> color
        // ... implementation details ...
    }
    else
    {
        // normal reading
        JSAMPARRAY buffer = (*state->cinfo.mem->alloc_sarray)
            ((j_common_ptr) &state->cinfo, JPOOL_IMAGE, state->cinfo.output_width * state->cinfo.num_components, 1);

        for( int i = 0; i < m_height; i++, data += step )
        {
            jpeg_read_scanlines( &state->cinfo, buffer, 1 );
            if( color )
            {
                if( state->cinfo.num_components == 3 )
                    icvCvt_BGR2RGB_8u_C3R( buffer[0], 0, data, 0, Size(m_width, 1) );
                else
                    icvCvt_Gray2BGR_8u_C1C3R( buffer[0], 0, data, 0, Size(m_width, 1) );
            }
            else
            {
                memcpy( data, buffer[0], m_width );
            }
        }
    }

    jpeg_finish_decompress( &state->cinfo );
    result = true;

    close();
    return result;
}

}

#endif
