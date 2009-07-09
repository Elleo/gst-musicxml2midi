/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2009 Michael Sheldon <mike@mikeasoft.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-musicxml2midi
 *
 * FIXME:Describe musicxml2midi here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m filesrc location=test.xml ! musicxml2midi ! wildmidi ! audioconvert ! autoaudiosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>

#include "gstmusicxml2midi.h"

GST_DEBUG_CATEGORY_STATIC (gst_musicxml2midi_debug);
#define GST_CAT_DEFAULT gst_musicxml2midi_debug

#define TIME_DIVISION 384

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/xml")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/midi")
    );

GST_BOILERPLATE (GstMusicXml2Midi, gst_musicxml2midi, GstElement,
    GST_TYPE_ELEMENT);

static void gst_musicxml2midi_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_musicxml2midi_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_musicxml2midi_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_musicxml2midi_chain (GstPad * pad, GstBuffer * buf);
static gboolean gst_musicxml2midi_sink_event (GstPad * pad, GstEvent * event);
static Track *get_track_by_part (GstMusicXml2Midi * filter, xmlChar * part_id);
static GstBuffer *process_element(GstMusicXml2Midi * filter, xmlNode * node);
static GstBuffer *process_partlist(GstMusicXml2Midi * filter, xmlNode * node);
static GstBuffer *process_part(GstMusicXml2Midi * filter, xmlNode * node);
static void process_score_part(GstMusicXml2Midi * filter, xmlNode * node);
static GstBuffer *process_attributes(GstMusicXml2Midi * filter, xmlNode * node);
static GstBuffer *process_time(GstMusicXml2Midi * filter, xmlNode * node);
static GstBuffer *process_key(GstMusicXml2Midi * filter, xmlNode * node);
static GstBuffer *process_note(GstMusicXml2Midi * filter, xmlNode * node, Track * track);

/* GObject vmethod implementations */

static void
gst_musicxml2midi_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "MusicXml2Midi",
    "audio",
    "Converts musicxml to midi format allowing playback via a standard midi synthesizer",
    "Michael Sheldon <mike@mikeasoft.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the musicxml2midi's class */
static void
gst_musicxml2midi_class_init (GstMusicXml2MidiClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_musicxml2midi_set_property;
  gobject_class->get_property = gst_musicxml2midi_get_property;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_musicxml2midi_init (GstMusicXml2Midi * filter,
    GstMusicXml2MidiClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_musicxml2midi_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_event_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR (gst_musicxml2midi_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_musicxml2midi_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  filter->ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);
  filter->first_track = NULL;
  filter->num_tracks = 0;
}

static void
gst_musicxml2midi_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
//  GstMusicXml2Midi *filter = GST_MUSICXML2MIDI (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_musicxml2midi_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
//  GstMusicXml2Midi *filter = GST_MUSICXML2MIDI (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_musicxml2midi_set_caps (GstPad * pad, GstCaps * caps)
{
  GstMusicXml2Midi *filter;
  GstPad *otherpad;

  filter = GST_MUSICXML2MIDI (gst_pad_get_parent (pad));
  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

/* Convert music xml to MIDI and push it to the src pad */
static GstBuffer *
process_element(GstMusicXml2Midi * filter, xmlNode * node)
{
  xmlNode *cur_node = NULL;
  GstBuffer *buf = NULL;
  GstBuffer *elem_buf;

  for (cur_node = node; cur_node; cur_node = cur_node->next) {
    elem_buf = NULL;
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrEqual(cur_node->name, (xmlChar *) "part")) {
        elem_buf = process_part(filter, cur_node);
      } else if (xmlStrEqual(cur_node->name, (xmlChar *) "part-list")) {
        elem_buf = process_partlist(filter, cur_node);
      } else {
        elem_buf = process_element(filter, cur_node->children);
      }
    }
    if (buf == NULL) {
      buf = elem_buf;
    } else if (elem_buf != NULL) {
      buf = gst_buffer_merge(buf, elem_buf);
    }
  }

  return buf;
}


static GstBuffer *
process_partlist(GstMusicXml2Midi * filter, xmlNode * node)
{
  xmlNode *child_node = node->children;
  int num_tracks = 0;
  GstBuffer *buf = gst_buffer_new_and_alloc(14);
  char *data = (char *) GST_BUFFER_DATA(buf);
  guint16 *data16 = (guint16 *) GST_BUFFER_DATA(buf);

  while (child_node != NULL) {
    if (xmlStrEqual(child_node->name, (xmlChar *) "score-part")) {
      process_score_part(filter, child_node);
      num_tracks++;
    }
    child_node = child_node->next;
  }

  /* Now we know about the tracks we can send the header */
  memset(data, 0, 14);
  data[0] = 'M'; data[1] = 'T'; data[2] = 'h'; data[3] = 'd'; /* MThd - MIDI File Header */
  data[7] = 6; /* Chunk size */
  data[9] = 1; /* Format */
  data[11] = num_tracks;
  data16[6] = g_htons(TIME_DIVISION);

  return buf;
}

static GstBuffer *
process_part(GstMusicXml2Midi * filter, xmlNode * node)
{
  xmlNode *child_node = node->children;
  xmlNode *measure_node;
  xmlChar *part_id = xmlGetProp(node, (xmlChar *) "id");
  GstBuffer *buf = gst_buffer_new_and_alloc(8);
  char *data = (char *) GST_BUFFER_DATA(buf);
  GstBuffer *note_buf = NULL;
  GstBuffer *tmp_buf = NULL;
  GstBuffer *end_buf = gst_buffer_new_and_alloc(4);
  guint8 *end_data = GST_BUFFER_DATA(end_buf);

  memset(data, 0, 8);
  data[0] = 'M'; data[1] = 'T'; data[2] = 'r'; data[3] = 'k'; /* MTrk - MIDI Track Header */
  end_data[0] = 0x00; end_data[1] = 0xff; end_data[2] = 0x2f; end_data[3] = 0x00; /* End of track event */

  Track *t = get_track_by_part(filter, part_id);
  if (t == NULL) {
    GST_WARNING("No score-part associated with this part. This part will not be heard.");
    return buf;
  }

  while (child_node != NULL) {
    if (xmlStrEqual(child_node->name, (xmlChar *) "measure")) {
        measure_node = child_node->children;
        while (measure_node != NULL) {
          tmp_buf = NULL;
          if (xmlStrEqual(measure_node->name, (xmlChar *) "attributes")) {
            tmp_buf = process_attributes(filter, measure_node);
            /* Set patch after attributes */
            GstBuffer *patch_buf = gst_buffer_new_and_alloc(3);
            guint8 *patch_data = GST_BUFFER_DATA(patch_buf);
            patch_data[0] = 0; /* Delta time */
            patch_data[1] = (0xc << 4) | t->midi_channel; /* Set patch on channel */
            patch_data[2] = t->midi_instrument;
            tmp_buf = gst_buffer_merge(tmp_buf, patch_buf);
          } else if (xmlStrEqual(measure_node->name, (xmlChar *) "note")) {
            tmp_buf = process_note(filter, measure_node, t);
          }
          
          if (note_buf == NULL) {
            note_buf = tmp_buf;
          } else if (tmp_buf != NULL) {
            note_buf = gst_buffer_merge(note_buf, tmp_buf);
          }
          measure_node = measure_node->next;
        }
    }
    child_node = child_node->next;
  }

  note_buf = gst_buffer_merge(note_buf, end_buf);

  guint32 *header = (guint32 *) GST_BUFFER_DATA(buf);
  header[1] = g_htonl(GST_BUFFER_SIZE(note_buf));

  buf = gst_buffer_merge(buf, note_buf);

  return buf;
}


static Track *
get_track_by_part(GstMusicXml2Midi * filter, xmlChar * part_id) {
  Track *t = filter->first_track;
  while(t != NULL && !xmlStrEqual(t->xml_id, part_id)) {
    t = t->next;
  }
  return t;
}


static void
process_score_part(GstMusicXml2Midi * filter, xmlNode * node)
{
  Track *t = malloc(sizeof(Track));
  Track *n = filter->first_track;
  xmlNode *child_node = node->children;
  xmlNode *midi_child;
  t->track_id = filter->num_tracks;
  t->volume = 127;
  t->midi_channel = 0;
  t->midi_instrument = 0;
  filter->num_tracks++;
  t->xml_id = xmlGetProp(node, (xmlChar *) "id");
  t->next = NULL;

  while (child_node != NULL) {
    if (xmlStrEqual(child_node->name, (xmlChar *) "midi-instrument")) {
      midi_child = child_node->children;
      while (midi_child != NULL) {
        if (xmlStrEqual(midi_child->name, (xmlChar *) "midi-channel")) {
          t->midi_channel = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, midi_child->xmlChildrenNode, 1));
        } else if (xmlStrEqual(midi_child->name, (xmlChar *) "midi-program")) {
          t->midi_instrument = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, midi_child->xmlChildrenNode, 1));
        }
        midi_child = midi_child->next;
      }
    }
    child_node = child_node->next;
  }

  if (filter->first_track == NULL) {
    filter->first_track = t;
  } else {
    while(n->next != NULL) {
      n = n->next;
    }
    n->next = t;
  }

}


static GstBuffer *
process_attributes(GstMusicXml2Midi * filter, xmlNode * node)
{
  xmlNode *child_node = node->children;
  GstBuffer *buf = NULL;
  GstBuffer *tmp_buf;

  while (child_node != NULL) {
    tmp_buf = NULL;
            
    if (xmlStrEqual(child_node->name, (xmlChar *) "time")) {
      tmp_buf = process_time(filter, child_node);
    } else if (xmlStrEqual(child_node->name, (xmlChar *) "key")) {
      tmp_buf = process_key(filter, child_node);
    }

    if (buf == NULL) {
      buf = tmp_buf;
    } else if (tmp_buf != NULL) {
      buf = gst_buffer_merge(buf, tmp_buf);
    }

    child_node = child_node->next;
  }

  return buf;
}


static GstBuffer *
process_time(GstMusicXml2Midi * filter, xmlNode * node)
{
  xmlNode *child_node = node->children;
  GstBuffer *buf = gst_buffer_new_and_alloc(8);
  guint8 *data = GST_BUFFER_DATA(buf);
  guint8 beats = 0;
  guint8 beat_type = 0;

  while (child_node != NULL) {
    if (xmlStrEqual(child_node->name, (xmlChar *) "beats")) {
      beats = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, child_node->xmlChildrenNode, 1));
    } else if (xmlStrEqual(child_node->name, (xmlChar *) "beat-type")) {
      beat_type = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, child_node->xmlChildrenNode, 1));
    }
    child_node = child_node->next;
  }

  if(beats != 0 && beat_type != 0) {
    data[0] = 0x00; /* Delta time */
    data[1] = 0xff; /* Meta event */
    data[2] = 0x58; /* Set tempo */
    data[3] = 4; /* Event data length */
    data[4] = beats;
    data[5] = beat_type;
    data[6] = 24; /* Metronome */
    data[7] = 8; /* 32nds */
    return buf;
  } else {
    return NULL;
  }
}


static GstBuffer *
process_key(GstMusicXml2Midi * filter, xmlNode * node)
{
  xmlNode *child_node = node->children;
  GstBuffer *buf = gst_buffer_new_and_alloc(6);
  guint8 *data = GST_BUFFER_DATA(buf);
  guint8 fifths = 0;

  while (child_node != NULL) {
    if (xmlStrEqual(child_node->name, (xmlChar *) "fifths")) {
      fifths = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, child_node->xmlChildrenNode, 1));
    }
    child_node = child_node->next;
  }

  data[0] = 0x00; /* Delta time */
  data[1] = 0xff; /* Meta event */
  data[2] = 0x59; /* Set key signature */
  data[3] = 2; /* Event data length */
  data[4] = fifths;
  data[5] = 0; /* Scale */

  return buf;
}


static GstBuffer *
process_note(GstMusicXml2Midi * filter, xmlNode * node, Track * track)
{
  xmlNode *child_node = node->children;
  xmlNode *pitch_child;
  GstBuffer *buf = gst_buffer_new_and_alloc(8);
  guint8 *data = GST_BUFFER_DATA(buf);
  guint8 duration = 0, pitch = 0, step = 0, octave = 0;
  gint8 alter = 0;
  gboolean rest = FALSE;

  while (child_node != NULL) {
    if (xmlStrEqual(child_node->name, (xmlChar *) "duration")) {
      duration = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, child_node->xmlChildrenNode, 1));
    } else if (xmlStrEqual(child_node->name, (xmlChar *) "rest")) {
      rest = TRUE;
    } else if (xmlStrEqual(child_node->name, (xmlChar *) "pitch")) {
      pitch_child = child_node->children;
      while (pitch_child != NULL) {
        if (xmlStrEqual(pitch_child->name, (xmlChar *) "step")) {
          step = (guint8) xmlNodeListGetString(filter->ctxt->myDoc, pitch_child->xmlChildrenNode, 1)[0]; 
          if (step < 72) {
            /* ASCII offset to capital A */
            step -= 65;
          } else {
            /* ASCII offset to lower case a */
            step -= 97;
          }
          printf("Step: %d\n", step);
        } else if (xmlStrEqual(pitch_child->name, (xmlChar *) "octave")) {
          octave = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, pitch_child->xmlChildrenNode, 1));
        } else if (xmlStrEqual(pitch_child->name, (xmlChar *) "alter")) {
          alter = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, pitch_child->xmlChildrenNode, 1));
        }
        pitch_child = pitch_child->next;
      }
      pitch = (12 * (octave + 1)) + step + alter - 2; /* Convert to MIDI note numbers */
    } 
    child_node = child_node->next;
  }

  if (rest) {
    
  } else {
    /* Note on */
    data[0] = 0x00;
    data[1] = (0x9 << 4) | track->midi_channel;
    data[2] = pitch;
    data[3] = track->volume;
    /* Note off */
    data[4] = duration;
    data[5] = (0x8 << 4) | track->midi_channel;
    data[6] = pitch;
    data[7] = 0;
  }

  return buf;
}


static gboolean
gst_musicxml2midi_sink_event (GstPad * pad, GstEvent * event)
{
  if (GST_EVENT_TYPE(event) == GST_EVENT_EOS) {
    GstMusicXml2Midi *filter = GST_MUSICXML2MIDI (gst_pad_get_parent (pad));
    xmlNode *root = xmlDocGetRootElement(filter->ctxt->myDoc);
    GstBuffer *buf = process_element(filter, root);
    gst_pad_push (filter->srcpad, buf);
  } 
  return TRUE;
}


/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_musicxml2midi_chain (GstPad * pad, GstBuffer * buf)
{
  GstMusicXml2Midi *filter;
  xmlNode *root_element = NULL;

  filter = GST_MUSICXML2MIDI (gst_pad_get_parent (pad));

  xmlParseChunk(filter->ctxt, (char *) buf->data, buf->size, 0);
  root_element = xmlDocGetRootElement(filter->ctxt->myDoc);

  return GST_FLOW_OK;

//  return gst_pad_push (filter->srcpad, buf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
musicxml2midi_init (GstPlugin * musicxml2midi)
{
  /* debug category for fltering log messages
   *
   * exchange the string ' musicxml2midi' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_musicxml2midi_debug, "musicxml2midi",
      0, " musicxml2midi");

  return gst_element_register (musicxml2midi, "musicxml2midi", GST_RANK_NONE,
      GST_TYPE_MUSICXML2MIDI);
}

/* gstreamer looks for this structure to register musicxml2midis
 *
 * exchange the string ' musicxml2midi' with your musicxml2midi description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "musicxml2midi",
    "Converts musicxml to midi format allowing playback via a standard midi synthesizer",
    musicxml2midi_init,
    VERSION,
    "LGPL",
    "Michael Sheldon <mike@mikeasoft.com>",
    "http://www.mikeasoft.com"
)
