#include <zmq.h>
#include <gst/gst.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _App App;

struct _App
{
  GstElement *pipeline;
  GstElement *appsrc;

  GMainLoop *loop;
  guint sourceid;

  GTimer *timer;

};

void *context;
void *subscriber;

App s_app;

static gboolean
read_data (App * app)
{
    gsize len;
    GstFlowReturn ret;
    gdouble ms;

    ms = g_timer_elapsed(app->timer, NULL);
    if (ms > 1.0/20.0) {
        GstBuffer *buffer;
        GdkPixbuf *pb;
        gboolean ok = TRUE;

        pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 160, 120);
        gdk_pixbuf_fill(pb, 0x00);
		len = 120*160*2;
		buffer = gst_buffer_new_wrapped((gpointer)gdk_pixbuf_get_pixels(pb), len);
        g_signal_emit_by_name (app->appsrc, "push-buffer", buffer, &ret);
        gst_buffer_unref (buffer);

        if (ret != GST_FLOW_OK) {
            /* some error, stop sending data */
			g_printerr("an error in the flow has happened\n");
            ok = FALSE;
        }

        g_timer_start(app->timer);

        return ok;
    }

    //  g_signal_emit_by_name (app->appsrc, "end-of-stream", &ret);
    return FALSE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void
start_feed (GstElement * pipeline, guint size, App * app)
{
  if (app->sourceid == 0) {
    g_printerr("start feeding\n");
    app->sourceid = g_idle_add ((GSourceFunc) read_data, app);
  }
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void
stop_feed (GstElement * pipeline, App * app)
{
  if (app->sourceid != 0) {
    g_printerr ("stop feeding\n");
    g_source_remove (app->sourceid);
    app->sourceid = 0;
  }
}

static gboolean
bus_message (GstBus * bus, GstMessage * message, App * app)
{
  g_printerr("got message %s\n",
      gst_message_type_get_name (GST_MESSAGE_TYPE (message)));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
        GError *err = NULL;
        gchar *dbg_info = NULL;

        gst_message_parse_error (message, &err, &dbg_info);
        g_printerr ("ERROR from element %s: %s\n",
            GST_OBJECT_NAME (message->src), err->message);
        g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
        g_error_free (err);
        g_free (dbg_info);
        g_main_loop_quit (app->loop);
        break;
    }
    case GST_MESSAGE_EOS:
      g_main_loop_quit (app->loop);
      break;
    default:
      break;
  }
  return TRUE;
}

int
main (int argc, char *argv[])
{
  App *app = &s_app;
  GstBus *bus;

  gst_init (&argc, &argv);

  /* create a mainloop to get messages and to handle the idle handler that will
   * feed data to appsrc. */
  app->loop = g_main_loop_new (NULL, TRUE);
  app->timer = g_timer_new();
  //char* launcher = "appsrc name=mysource ! videoconvert ! x264enc tune='zerolatency' threads=1 ! mpegtsmux ! tcpserversink port=8554";
  char* launcher = "zmqsrc ! video/x-raw, format=GRAY8, width=160, height=120, framerate=16/1 ! autovideosink";
  app->pipeline = gst_parse_launch(launcher, NULL);

  g_assert (app->pipeline);

  bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
  g_assert(bus);

  /* add watch for messages */
  gst_bus_add_watch (bus, (GstBusFunc) bus_message, app);

  /* get the appsrc */
    //app->appsrc = gst_bin_get_by_name (GST_BIN(app->pipeline), "mysource");
    //g_assert(app->appsrc);
    //g_signal_connect (app->appsrc, "need-data", G_CALLBACK (start_feed), app);
    //g_signal_connect (app->appsrc, "enough-data", G_CALLBACK (stop_feed), app);

  /* set the caps on the source */

  /*g_object_set(G_OBJECT (app->appsrc),
		"stream-type", 0,
		"format", GST_FORMAT_TIME,
		"caps", gst_caps_new_simple ( "video/x-raw",
			"format", G_TYPE_STRING, "GRAY8",
			"framerate", GST_TYPE_FRACTION, 16, 1,
			"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
			"width", G_TYPE_INT, 160,
			"height", G_TYPE_INT, 120,
		NULL), NULL);
*/

  /* go to playing and wait in a mainloop. */
  gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

  /* this mainloop is stopped when we receive an error or EOS */
  g_printerr("starting\n");
  g_main_loop_run (app->loop);

  g_printerr("stopping\n");

  gst_element_set_state (app->pipeline, GST_STATE_NULL);

  gst_object_unref (bus);
  g_main_loop_unref (app->loop);

  return 0;
}
