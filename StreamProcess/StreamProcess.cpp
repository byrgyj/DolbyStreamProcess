// StreamProcess.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
// #include <libavutil/timestamp.h>
// #include <libavformat/avformat.h>
};
#define LOGI printf

#define AV_Q2D(a) (a.num / (double) a.den)

int demuxerAndRemuxer();

void connectAudioData();

int parseVideo();

int _tmain(int argc, _TCHAR* argv[])
{
	av_register_all(); 
	//connectAudioData();
	demuxerAndRemuxer();

	//parseVideo();
	return 0;
}


int demuxerAndRemuxer()  
{  
	AVOutputFormat *ofmt = NULL;  
	AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL,*ofmt_ctx = NULL;  
	AVPacket pkt;  
	int ret, i;  
	int videoindex_v=-1,videoindex_out=-1;  
	int audioindex_a=-1,audioindex_out=-1;  
	int frame_index=0;  
	int64_t cur_pts_v=0,cur_pts_a=0;  

	const char *in_filename_v = "dolby/591bd952b6f0d7460d36d9a5c8c91f38 (1).ts";//Input file URL  
	const char *in_filename_a = "dolby/1083664_1700112.mp4";  
	const char *out_filename = "out.ts";//Output file URL  


	//Input  
	if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {  
		LOGI( "Could not open input file.");  
		goto end;  
	}  
	if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {  
		LOGI( "Failed to retrieve input stream information");  
		goto end;  
	}  

	if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) {  
		LOGI( "Could not open input file.");  
		goto end;  
	}  
	if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {  
		LOGI( "Failed to retrieve input stream information");  
		goto end;  
	}  

	AVStream *vs = ifmt_ctx_a->streams[0];
	int duration = vs->duration;
	int totalSec = duration/vs->time_base.den;
	AVCodecContext *codec = vs->codec;
	int bitRate = codec->bit_rate;


	LOGI("===========Input Information==========\n");  
	av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);  
	av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);  
	LOGI("======================================\n");  
	//Output  
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);  
	if (!ofmt_ctx) {  
		LOGI( "Could not create output context\n");  
		ret = AVERROR_UNKNOWN;  
		goto end;  
	}  
	ofmt = ofmt_ctx->oformat;  

	for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
		if(ifmt_ctx_v->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){  
			AVStream *in_stream = ifmt_ctx_v->streams[i];  
			AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);  
			videoindex_v=i;  
			if (!out_stream) {  
				LOGI( "Failed allocating output stream\n");  
				ret = AVERROR_UNKNOWN;  
				goto end;  
			}  
			videoindex_out=out_stream->index;  
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {  
				LOGI( "Failed to copy context from input to output stream codec context\n");  
				goto end;  
			}  
			out_stream->codec->codec_tag = 0;  
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)  
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;  
			break;  
		}  
	}  

	for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {  
		if(ifmt_ctx_a->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){  
			AVStream *in_stream = ifmt_ctx_a->streams[i];  
			AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);  
			audioindex_a=i;  
			if (!out_stream) {  
				LOGI( "Failed allocating output stream\n");  
				ret = AVERROR_UNKNOWN;  
				goto end;  
			}  
			audioindex_out=out_stream->index;  
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {  
				LOGI( "Failed to copy context from input to output stream codec context\n");  
				goto end;  
			}  
			out_stream->codec->codec_tag = 0;  
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)  
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;  

			break;  
		}  
	}  

	LOGI("==========Output Information==========\n");  
	av_dump_format(ofmt_ctx, 0, out_filename, 1);  
	LOGI("======================================\n");  
	//Open output file  
	if (!(ofmt->flags & AVFMT_NOFILE)) {  
		if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {  
			LOGI( "Could not open output file '%s'", out_filename);  
			goto end;  
		}  
	}  
	//Write file header  
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {  
		LOGI( "Error occurred when opening output file\n");  
		goto end;  
	}  

	while (1) {  
		AVFormatContext *ifmt_ctx;  
		int stream_index=0;  
		AVStream *in_stream, *out_stream;  

		if(av_compare_ts(cur_pts_v,ifmt_ctx_v->streams[videoindex_v]->time_base,cur_pts_a,ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){  
			ifmt_ctx=ifmt_ctx_v;  
			stream_index=videoindex_out;  

			if(av_read_frame(ifmt_ctx, &pkt) >= 0){  
				do{  
					in_stream  = ifmt_ctx->streams[pkt.stream_index];  
					out_stream = ofmt_ctx->streams[stream_index];  

					if(pkt.stream_index==videoindex_v){  
						//FIX：No PTS (Example: Raw H.264)  
						if(pkt.pts==AV_NOPTS_VALUE){  
							AVRational time_base1=in_stream->time_base;  
							int64_t calc_duration=(double)AV_TIME_BASE/AV_Q2D(in_stream->r_frame_rate);  
							pkt.pts=(double)(frame_index*calc_duration)/(double)(AV_Q2D(time_base1)*AV_TIME_BASE);  
							pkt.dts=pkt.pts;  
							pkt.duration=(double)calc_duration/(double)(AV_Q2D(time_base1)*AV_TIME_BASE);  
							frame_index++;  
						}  

						cur_pts_v=pkt.pts;  

						//LOGI("time:%f\n", pkt.dts * av_q2d(in_stream->time_base));

						break;  
					}  
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0);  
			}else{  
				break;  
			}  
		}else{  
			ifmt_ctx=ifmt_ctx_a;  
			stream_index=audioindex_out;  
			if(av_read_frame(ifmt_ctx, &pkt) >= 0){  
				do{  
					in_stream  = ifmt_ctx->streams[pkt.stream_index];  
					out_stream = ofmt_ctx->streams[stream_index];  

					if(pkt.stream_index==audioindex_a){  

		
						if(pkt.pts==AV_NOPTS_VALUE){  
							AVRational time_base1=in_stream->time_base;  
							int64_t calc_duration=(double)AV_TIME_BASE/AV_Q2D(in_stream->r_frame_rate);  
							//Parameters  
							pkt.pts=(double)(frame_index*calc_duration)/(double)(AV_Q2D(time_base1)*AV_TIME_BASE);  
							pkt.dts=pkt.pts;  
							pkt.duration=(double)calc_duration/(double)(AV_Q2D(time_base1)*AV_TIME_BASE);  
							frame_index++;  
						}  
						cur_pts_a=pkt.pts;  
						LOGI("audio pts=%d\n", pkt.pts);
						break;  
					}  
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0);  
			}else{  
				break;  
			}  

		}  

		//Convert PTS/DTS  
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));  
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));  
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);  
		pkt.pos = -1;  
		pkt.stream_index=stream_index;  

		//LOGI("Write 1 Packet. size:%5d",pkt.size);  
		//LOGI("Write 1 Packet.pts--->%11d",pkt.pts);  
		//Write  
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {  
			LOGI( "Error muxing packet\n");  
			break;  
		}  
		av_free_packet(&pkt);  

	}  

	av_write_trailer(ofmt_ctx);  

end:  
	avformat_close_input(&ifmt_ctx_v);  
	avformat_close_input(&ifmt_ctx_a);  
	/* close output */  
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))  
		avio_close(ofmt_ctx->pb);  
	avformat_free_context(ofmt_ctx);  
	if (ret < 0 && ret != AVERROR_EOF) {  
		LOGI( "Error occurred.\n");  
		return -1;  
	}  
	return 0;  
}  


void connectAudioData()
{
	FILE *dest = fopen("dolby.mp4", "wb+");
	int length = 0;

	FILE *fileHeader = fopen("test/header.mp4", "rb");
	if (fileHeader != NULL){
		uint8_t szData[1024*2]= { 0 };
		int length = fread(szData, 1, sizeof(szData), fileHeader);

		if (length > 0){
			fwrite(szData, 1, length, dest);
		}
		fclose(fileHeader);
	}

	FILE *fileData = fopen("test/1.amp4", "rb");
	if (fileData != NULL){
		while(!feof(fileData))
		{
			uint8_t szData[1024*2] = { 0 };
			length = fread(szData, 1, sizeof(szData), fileData);
			fwrite(szData, 1, length, dest);
		}

		fclose(fileData);
	}
	fclose(dest);
}

int parseVideo(){
	AVFormatContext *ctx = NULL;

	char *video = "D:\\MyProg\\StreamProcess\\StreamProcess\\419cb3acc3844c0106238ec19087e326.f4v";
	int ret = avformat_open_input(&ctx, video, NULL, NULL);
	ret = avformat_find_stream_info(ctx, NULL);

	for (int i=0; i < ctx->nb_streams; i++){
		AVStream *stream = ctx->streams[i];
		AVRational tb  = stream->time_base;
		printf("timebase.den:%d, timebase.num:%d", tb.den, tb.num);
	}


	return ret;

}