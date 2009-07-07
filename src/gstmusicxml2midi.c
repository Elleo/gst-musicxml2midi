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

#include "gstmusicxml2midi.h"

GST_DEBUG_CATEGORY_STATIC (gst_musicxml2midi_debug);
#define GST_CAT_DEFAULT gst_musicxml2midi_debug

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
static GstBuffer *process_note(GstMusicXml2Midi * filter, xmlNode * node);

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
  GstBuffer *elem_buf = NULL;

  for (cur_node = node; cur_node; cur_node = cur_node->next) {
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
  GstBuffer *buf = gst_buffer_new_and_alloc(15);
  char *data = (char *) GST_BUFFER_DATA(buf);

  while (child_node != NULL) {
    if (xmlStrEqual(child_node->name, (xmlChar *) "score-part")) {
      process_score_part(filter, child_node);
      num_tracks++;
    }
    child_node = child_node->next;
  }

  /* Now we know about the tracks we can send the header */
  
  data[0] = 'M'; data[1] = 'T'; data[2] = 'h'; data[3] = 'd'; /* MThd - MIDI Track Header */
  data[7] = 6; /* Chunk size */
  data[9] = 1; /* Format */
  data[11] = num_tracks;
  data[13] = 48; /* Time division */

  printf("Header: %s\n", data);
  return buf;
}

static GstBuffer *
process_part(GstMusicXml2Midi * filter, xmlNode * node)
{
  xmlNode *child_node = node->children;
  xmlNode *measure_node;
  xmlChar *part_id = xmlGetProp(node, (xmlChar *) "id");
  GstBuffer *buf = gst_buffer_new_and_alloc(4);
  Track *t = get_track_by_part(filter, part_id);
  if (t == NULL) {
    GST_WARNING("No score-part associated with this part. This part will not be heard.");
    return buf;
  }

  printf("Track: %d\n", t->track_id);

  while (child_node != NULL) {
    if (xmlStrEqual(child_node->name, (xmlChar *) "measure")) {
        measure_node = child_node->children;
        while (measure_node != NULL) {
          if (xmlStrEqual(measure_node->name, (xmlChar *) "note")) {
            buf = gst_buffer_merge(buf, process_note(filter, measure_node));
          }
          measure_node = measure_node->next;
        }
    }
    child_node = child_node->next;
  }

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
process_note(GstMusicXml2Midi * filter, xmlNode * node)
{
  xmlNode *child_node = node->children;
  xmlNode *pitch_child;
  GstBuffer *buf = gst_buffer_new_and_alloc(2);
  int duration = 0, pitch = 0, step = 0, octave = 0;
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
          step = (int) xmlNodeListGetString(filter->ctxt->myDoc, pitch_child->xmlChildrenNode, 1)[0]; 
        } else if (xmlStrEqual(pitch_child->name, (xmlChar *) "octave")) {
          octave = atoi((char *) xmlNodeListGetString(filter->ctxt->myDoc, pitch_child->xmlChildrenNode, 1));
        }
        pitch_child = pitch_child->next;
      }
      pitch = (8 * octave) + step;
    } 
    child_node = child_node->next;
  }

  if (rest) {
    printf("Rest, duration: %d\n", duration);
  } else {
    printf("Note, pitch: %d, duration: %d\n", pitch, duration);
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
