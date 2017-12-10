#include <alsa/asoundlib.h>
#include <alsa/seq.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#define DEFAULT_NAME	"MIDI Out"

static snd_seq_t *seq_handle = NULL;

static snd_seq_event_t ev;
static int my_client, my_port;
static int seq_client=0, seq_port=0;

bool isRun=true;

termios oldtio,newtio;
int fd1, fd2;
fd_set readfs;    /* file descriptor set */
int    maxfd;     /* maximum file desciptor used */
int serial_init(){       
        /* open_input_source opens a device, sets the port correctly, and returns a file descriptor */
        fd1 = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY );
        if (fd1<0){
				puts("USB-TTL Converter not found!");
				return 0;
			}
        fd2=STDIN_FILENO;
        tcgetattr(fd1,&oldtio); /* save current serial port settings */
        tcgetattr(fd1,&newtio); /* save current serial port settings */
        cfmakeraw(&newtio);
        cfsetspeed(&newtio,31250);
        tcsetattr(fd1,TCSANOW,&newtio);
        maxfd = MAX(fd1, fd2)+1;  /* maximum bit entry (fd) to test */
      }

char midi_buf[3];
int notesn=0;
int pgm=0;

int main(int argc,char** argv){
	puts("\nMIDI keyboard connection\n");
  if(argc>1){
    sscanf(argv[1],"%d",&pgm);
  }
  int l=0;
  fd_set rfds;
  jack_client_t *client;
  jack_status_t status;
  if (snd_seq_open(&seq_handle, "hw", SND_SEQ_OPEN_OUTPUT, 0) >= 0) {
	snd_seq_set_client_name(seq_handle, "Serial2MIDI");
//	snd_seq_set_client_group(seq_handle, "input");

	my_port = snd_seq_create_simple_port(seq_handle, DEFAULT_NAME, SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
	snd_seq_client_info_t *ptr;
	int cl=-1;
	if(snd_seq_client_info_malloc(&ptr)>=0){
	  while(snd_seq_query_next_client(seq_handle,ptr)==0){
	    if(strcmp(snd_seq_client_info_get_name(ptr),"FLUID Synth (qsynth)")==0){
	      cl=snd_seq_client_info_get_client(ptr);
  	    printf("QSynth found at index %d\n",cl);
  	    break;
	    }
	  }
	  snd_seq_client_info_free(ptr);
	}
	snd_seq_connect_to(seq_handle, my_port, cl, 0);
    if (my_port>=0) {

// set program
                snd_seq_ev_set_pgmchange(&ev, 0, pgm);
                snd_seq_ev_set_direct(&ev);
                snd_seq_ev_set_broadcast(&ev);
                snd_seq_ev_set_source(&ev, my_port);
                snd_seq_ev_set_dest(&ev, SND_SEQ_ADDRESS_SUBSCRIBERS, 0);
                snd_seq_event_output_direct(seq_handle, &ev);

      if(serial_init()){
        while (isRun) {
            l=read(fd1,midi_buf+notesn,1);
            if(l==1){
              if(notesn>0)if((midi_buf[notesn]&128)==128){
                midi_buf[0]=midi_buf[notesn];
                notesn=0;
              }
              notesn++;
            }
            if(notesn==3){
              if((midi_buf[0]&0xf0)==0x90)snd_seq_ev_set_noteon(&ev, 0, midi_buf[1], midi_buf[2]);//midi_buf[0]&0xf
              if((midi_buf[0]&0xf0)==0x80)snd_seq_ev_set_noteoff(&ev, 0, midi_buf[1], midi_buf[2]);
              snd_seq_ev_set_direct(&ev);
              snd_seq_ev_set_broadcast(&ev);
              snd_seq_ev_set_source(&ev, my_port);
              snd_seq_ev_set_dest(&ev, SND_SEQ_ADDRESS_SUBSCRIBERS, 0);
              snd_seq_event_output_direct(seq_handle, &ev);
//              printf("Note %s %3.3d (%3.3d)\n",((midi_buf[0]&0xf0)==0x90)?"on ":"off",midi_buf[1], midi_buf[2]);
              notesn=0;
            }
        }
        tcsetattr(fd1,TCSANOW,&oldtio);
        close(fd1);
      }
    }else{
      fprintf (stderr, "Port creation error\n");
    }
    snd_seq_close(seq_handle);
  }else{
    fprintf (stderr, "Synth opening error\n");
  }
}
