
#include <gst/gst.h>

#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
        GstElement *element;
        GstElement *appsrc;
        guint sourceid ;
        GstClockTime timestamp;
} MyContext;
#define BUFF_SIZE (800*304*3)

FILE *file=NULL;
gpointer ptr=NULL;
/* called when we need to give data to appsrc */
//static void
//need_data (GstElement * appsrc, guint unused, MyContext * ctx)
static gboolean read_data(MyContext *ctx)
{
        GstBuffer *buffer;
        GstFlowReturn gstret;

        size_t ret = 0;
        GstMapInfo map;
        memset(ptr,0,BUFF_SIZE);
        // printf(" . ");
        buffer = gst_buffer_new_allocate (NULL, BUFF_SIZE, NULL);


        gst_buffer_map (buffer, &map, GST_MAP_WRITE);
        ret = fread(ptr, 1, BUFF_SIZE, file);
        map.size = BUFF_SIZE;
        memcpy(map.data,ptr,BUFF_SIZE);

        if(ret > 0)
        {
                gstret = gst_app_src_push_buffer((GstAppSrc *)ctx->appsrc, buffer);
                if(gstret !=  GST_FLOW_OK){
                        printf("push buffer returned %d \n", gstret);
                        return FALSE;
                }

                // g_signal_emit_by_name (ctx->appsrc, "push-buffer", buffer, &gstret);
        }
        else
        {

                printf("\n failed to read from file\n");
                return FALSE;
        }

        gst_buffer_unmap (buffer, &map);
        return TRUE;
}



static void start_feed (GstElement * pipeline, guint size, MyContext *ctx)
{
        if (ctx->sourceid == 0) {
                printf ("\nstart feeding ");
                ctx->sourceid = g_idle_add ((GSourceFunc) read_data, ctx);
        }
}

static void stop_feed (GstElement * pipeline,MyContext *ctx)
{
        if (ctx->sourceid != 0) {
                printf ("\nstop feeding");
                g_source_remove (ctx->sourceid);
                ctx->sourceid = 0;
        }
}


/* called when a new media pipeline is constructed. We can query the
 * pipeline and configure our appsrc */
static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media,
    gpointer user_data)
{

        MyContext *ctx;

        ctx = g_new0 (MyContext, 1);
        /* get the element used for providing the streams of the media */
        ctx->element = gst_rtsp_media_get_element (media);

        /* get our appsrc, we named it 'mysrc' with the name property */
        ctx->appsrc = gst_bin_get_by_name_recurse_up (GST_BIN (ctx->element),
"mysrc");

        /* this instructs appsrc that we will be dealing with timed buffer */
// gst_util_set_object_arg (G_OBJECT (ctx->appsrc), "format", "time");
         g_object_set (G_OBJECT (ctx->appsrc),"format",GST_FORMAT_TIME,NULL);
        /* configure the caps of the video */
//        g_object_set (G_OBJECT (ctx->appsrc), "caps",
//                        gst_caps_new_simple ("video/x-h264",
//                                "format", G_TYPE_STRING, "RGB16",
//                                "width", G_TYPE_INT, 800,
//                                "height", G_TYPE_INT, 608,
//                                "framerate", GST_TYPE_FRACTION, 30, 1, NULL), NULL);

//          gst_util_set_object_arg (G_OBJECT (ctx->appsrc), "stream-type", "seekable");

        /* make sure ther datais freed when the media is gone */
        g_object_set_data_full (G_OBJECT (media), "my-extra-data", ctx,
                        (GDestroyNotify) g_free);

        /* install the callback that will be called when a buffer is needed */
        g_signal_connect (ctx->appsrc, "need-data", G_CALLBACK(start_feed), ctx);
        g_signal_connect(ctx->appsrc, "enough-data", G_CALLBACK(stop_feed), ctx);
}

int
main (int argc, char *argv[])
{
        GMainLoop *loop;
        GstRTSPServer *server;
        GstRTSPMountPoints *mounts;
        GstRTSPMediaFactory *factory;

        if(argc != 2){
                printf("File name not specified\n");
                return 1;
        }

        ptr = g_malloc(BUFF_SIZE);
        g_assert(ptr);
        file = fopen(argv[1], "r");

        gst_init (&argc, &argv);

        loop = g_main_loop_new (NULL, FALSE);

        /* create a server instance */
        server = gst_rtsp_server_new ();

        /* get the mount points for this server, every server has a default object
         * that be used to map uri mount points to media factories */
        mounts = gst_rtsp_server_get_mount_points (server);

        /* make a media factory for a test stream. The default media factory can
use
         * gst-launch syntax to create pipelines.
         * any launch line works as long as it contains elements named pay%d. Each
         * element with pay%d names will be a stream */
        factory = gst_rtsp_media_factory_new ();
        gst_rtsp_media_factory_set_launch (factory, "(appsrc is-live=TRUE name=mysrc do-timestamp=TRUE ! qtdemux name=mdemux ! h264parse ! video/x-h264,stream-format=byte-stream  ! rtph264pay   name=pay0 pt=96 config-interval=1 )");

        /* notify when our media is ready, This is called whenever someone asks for
         * the media and a new pipeline with our appsrc is created */
        g_signal_connect (factory, "media-configure", (GCallback) media_configure,
                        NULL);

        /* attach the test factory to the /test url */
        gst_rtsp_mount_points_add_factory (mounts, "/test", factory);

        /* don't need the ref to the mounts anymore */
        g_object_unref (mounts);

        /* attach the server to the default maincontext */
        gst_rtsp_server_attach (server, NULL);

        /* start serving */
        g_print ("stream ready at rtsp://127.0.0.1:8554/test\n");
        g_main_loop_run (loop);

        return 0;
}


