/***************************************************************************
                          soundsourceoggvorbis.cpp  -  ogg vorbis decoder
                             -------------------
    copyright            : (C) 2003 by Svein Magne Bang
    email                :
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "trackinfoobject.h"
#include "soundsourceoggvorbis.h"
#include <QtDebug>
#ifdef __WIN32__
#include <io.h>
#include <fcntl.h>
#endif

#ifdef __APPLE__
#ifdef __i386
#define OV_ENDIAN_ARG 0
#else
#define OV_ENDIAN_ARG 1
#endif
#endif

#ifdef __LINUX__
#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define OV_ENDIAN_ARG 0
#else
#define OV_ENDIAN_ARG 1
#endif
#else
#define OV_ENDIAN_ARG 0
#endif

/*
   Class for reading Ogg Vorbis
 */

SoundSourceOggVorbis::SoundSourceOggVorbis(QString qFilename) 
: SoundSource(qFilename)
, dest (0)
, pRead(0)
{
    QByteArray qBAFilename = qFilename.toUtf8();

#ifdef __WIN32__
    if(ov_fopen(qBAFilename.data(), &vf) < 0) {
        qDebug() << "oggvorbis: Input does not appear to be an Ogg bitstream.";
        filelength = 0;
        return;
    }
#else 
    FILE *vorbisfile =  fopen(qBAFilename.data(), "r");

    if (!vorbisfile) {
        qDebug() << "oggvorbis: cannot open" << qFilename;
        return;
    }

    if(ov_open(vorbisfile, &vf, NULL, 0) < 0) {
        qDebug() << "oggvorbis: Input does not appear to be an Ogg bitstream.";
        filelength = 0;
        return;
    }
#endif

    // extract metadata
    vorbis_info * vi=ov_info(&vf,-1);

    channels = vi->channels;
    SRATE = vi->rate;

    if(channels > 2){
        qDebug() << "oggvorbis: No support for more than 2 channels!";
        ov_clear(&vf);
        filelength = 0;
        return;
    }

    filelength = (unsigned long) ov_pcm_total(&vf, -1) * 2;
    if (filelength == OV_EINVAL) {
        //The file is not seekable. Not sure if any action is needed.
    }

}

SoundSourceOggVorbis::~SoundSourceOggVorbis()
{
    if (filelength > 0){
        ov_clear(&vf);
    }
}


/*
   seek to <filepos>
 */

long SoundSourceOggVorbis::seek(long filepos)
{
    Q_ASSERT(filepos%2==0);
    
    if (ov_seekable(&vf)){
        if(ov_pcm_seek(&vf, filepos >> 1) != 0) {
            qDebug() << "ogg vorbis: Seek ERR on seekable.";
        }

        // Even if an error occured, return them the current position
        // because that's what we promised.
        return ov_pcm_tell(&vf) << 1;
    } else{
        qDebug() << "ogg vorbis: Seek ERR.";
        return 0;
    }
}


/*
   read <size> samples into <destination>, and return the number of
   samples actually read.
 */

unsigned SoundSourceOggVorbis::read(volatile unsigned long size, const SAMPLE * destination)
{

    Q_ASSERT(size%2==0);
    
    pRead  = (char*) destination;
    dest   = (SAMPLE*) destination;
    
    index  = ret = 0;

    // 'needed' is size of buffer in bytes. 'size' is size in SAMPLEs,
    // which is 2 bytes.  If the stream is mono, we read 'size' bytes,
    // so that half the buffer is full, then below we double each
    // sample on the left and right channel. If the stream is stereo,
    // then ov_read interleaves the samples into the full length of
    // the buffer. 
    //needed = size*channels;

    // The above comment is wrong.
    if(channels == 2) {
        // size is the maximum space we have in destination, so only
        // read that many samples.
        needed = size;
    } else if(channels == 1) {
        // We will read size/2 samples and double them into
        // stereo. size is the maximum space we have in destination,
        // so respect that.
        needed = size/2;
    } else {
        // We don't support files with more than 2 channels.
        qDebug() << "SoundSourceOggVorbis :: Mixxx does not support audio with more than two audio channels.";
        return 0;
    }

    // loop until requested number of samples has been retrieved
    while (needed > 0) {
        index  += ret;
        needed -= ret;

        // read samples into buffer
        ret = ov_read(&vf, pRead+index, needed, OV_ENDIAN_ARG, 2, 1, &current_section);
        
        if (ret <= 0) {
            // An error or EOF occured, break out and return what we have sofar.
            break;
        }
    }

    // convert into stereo if file is mono
    if (channels == 1) {
        /*
        // index should always be divisible by two because it's
        // counting bytes when our word size is 2
        for(int i=(index/2); i>0; i--) {
            dest[i*2]     = dest[i];
            dest[(i*2)+1] = dest[i];
        }
        */

        
        // rryan 2/2009
        // Mini-proof of the below:
        // size = 20, destination is a 20 element array 0-19
        // readNo = 10 (or less, but 10 in this case)
        // i = 10-1 = 9, so dest[9*2] and dest[9*2+1],
        // so the first iteration touches the very ends of destination
        // on the last iteration, dest[0] and dest[1] are assigned to dest[0]
        
        for(int i=(index-1); i>=0; i--) {
            dest[i*2]     = dest[i];
            dest[(i*2)+1] = dest[i];
        }

        // We doubled the index bytes we read into stereo.
        return index*2;
    }

    // return the number of samples in buffer
    return index;
}

/*
   Parse the the file to get metadata
 */

int SoundSourceOggVorbis::ParseHeader( TrackInfoObject * Track )
{
    QString filename = Track->getLocation();
    QByteArray qBAFilename = filename.toUtf8();
    vorbis_comment *comment = NULL;
    OggVorbis_File vf;

#ifdef __WIN32__
    if (ov_fopen(qBAFilename.data(), &vf) < 0) {
        qDebug() << "oggvorbis: Input does not appear to be an Ogg bitstream.";        
        return ERR;
    }
#else
    FILE * vorbisfile = fopen(qBAFilename.data(), "r");

    if (!vorbisfile) {
        qDebug() << "oggvorbis: file cannot be opened.\n";
        return ERR;
    }

    if (ov_open(vorbisfile, &vf, NULL, 0) < 0) {
        qDebug() << "oggvorbis: Input does not appear to be an Ogg bitstream.\n";
        return ERR;
    }
#endif



    comment = ov_comment(&vf, -1);
    if (comment == NULL) {
        qDebug() << "oggvorbis: fatal error reading file.";
        ov_clear(&vf);
        return ERR;
    }

    if (QString(vorbis_comment_query(comment, "title", 0)).length()!=0)
        Track->setTitle(vorbis_comment_query(comment, "title", 0));
    if (QString(vorbis_comment_query(comment, "artist", 0)).length()!=0)
        Track->setArtist(vorbis_comment_query(comment, "artist", 0));
    if (QString(vorbis_comment_query(comment, "TBPM", 0)).length()!=0) {
        float bpm = str2bpm(vorbis_comment_query(comment, "TBPM", 0));
        if(bpm > 0.0f) {
            Track->setBpm(bpm);
            Track->setBpmConfirm(true);
        }
    }
    Track->setHeaderParsed(true);

    Track->setType("ogg");
    int duration = (int)ov_time_total(&vf, -1);
    if (duration == OV_EINVAL)
        return ERR;
    Track->setDuration(duration);
    Track->setBitrate(ov_bitrate(&vf, -1)/1000);

    vorbis_info *vi=ov_info(&vf,-1);
    if (vi)
    {
        Track->setSampleRate(vi->rate);
        Track->setChannels(vi->channels);
    }
    else
    {
        qDebug() << "oggvorbis: fatal error reading file.";
        ov_clear(&vf);
        return ERR;
    }
    
    ov_clear(&vf);
    return OK;
}

/*
   Return the length of the file in samples.
 */

inline long unsigned SoundSourceOggVorbis::length()
{
    return filelength;
}
