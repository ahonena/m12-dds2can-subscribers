#include <stdio.h>
//#include "subscribe_and_write2CAN.hpp"
#include "M12_Commands.h"
#include "M12_CommandsPlugin.h"
#include "M12_CommandsSupport.h"
#include "ndds/ndds_c.h"
//#include "ndds/ndds_utility_c.h"

#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define TRUE 1
#define FALSE 0

static volatile sig_atomic_t running = 1;
static void interrupt_handler(int sig)
{
  running = FALSE;
}

int main(){




  /* ---------------------------------------------------------------------------*/
  // DDS INITIALIZATION BEGIN
  // Based on RTI Connext C api documentation

  DDS_DomainParticipantFactory* factory = NULL;
  factory = DDS_DomainParticipantFactory_get_instance();
  if (factory == NULL) {
    printf("***Error: failed to get domain participant factory\n");
  }

  struct DDS_DomainParticipantQos participant_qos =
  DDS_DomainParticipantQos_INITIALIZER;
  DDS_DomainParticipant* participant;
  struct DDS_DomainParticipantListener participant_listener =
  DDS_DomainParticipantListener_INITIALIZER;
  DDS_ReturnCode_t retcode;
  const char* NDDS_DISCOVERY_INITIAL_PEERS[] = {
  };
  const long NDDS_DISCOVERY_INITIAL_PEERS_LENGTH =
  sizeof(NDDS_DISCOVERY_INITIAL_PEERS)/sizeof(const char*);


  // initialize participant_qos with default values
  retcode = DDS_DomainParticipantFactory_get_default_participant_qos(factory, &participant_qos);
  if (retcode != DDS_RETCODE_OK) {
    printf("***Error: failed to get default participant qos\n");
  }
  if (!DDS_StringSeq_from_array(&participant_qos.discovery.initial_peers,NDDS_DISCOVERY_INITIAL_PEERS,  NDDS_DISCOVERY_INITIAL_PEERS_LENGTH)) {
    printf("***Error: failed to set discovery.initial_peers qos\n");
  }

  // Create the participant
  int domain_id = 0;
  participant =
  DDS_DomainParticipantFactory_create_participant(factory,domain_id,  &participant_qos,  NULL,DDS_STATUS_MASK_ALL);
  if (participant == NULL) {
    printf("***Error: failed to create domain participant\n");
    return -1;
  }



  // Create the subscriber
  struct DDS_SubscriberQos subscriber_qos = DDS_SubscriberQos_INITIALIZER;
  DDS_Subscriber* subscriber;
  struct DDS_SubscriberListener subscriber_listener = DDS_SubscriberListener_INITIALIZER;
  retcode = DDS_DomainParticipant_get_default_subscriber_qos(participant,  &subscriber_qos);
  if (retcode != DDS_RETCODE_OK) {
    printf("***Error: failed to get default subscriber qos\n");
  }
  subscriber =  DDS_DomainParticipant_create_subscriber(participant,  &subscriber_qos,NULL,DDS_STATUS_MASK_ALL);
  if (subscriber == NULL) {
    printf("***Error: failed to create subscriber\n");
  }


  // Register and create the topic
  const char* type_name = "M12_Commands";
  retcode = M12_CommandsTypeSupport_register_type(participant, type_name);
  if (retcode != DDS_RETCODE_OK) {
    printf("***Error: failed to register type\n");
  }

  const char* topic_name = "M12_Commands";
  const char* type_type = "M12_Commands";
  struct DDS_TopicQos topic_qos = DDS_TopicQos_INITIALIZER;
  DDS_Topic* topic;
  struct DDS_TopicListener topic_listener = DDS_TopicListener_INITIALIZER;
  retcode = DDS_DomainParticipant_get_default_topic_qos(participant, &topic_qos);
  if (retcode != DDS_RETCODE_OK) {
    printf("***Error: failed to get default topic qos\n");
  }
  topic = DDS_DomainParticipant_create_topic(participant,topic_name, type_name, &topic_qos,NULL, DDS_STATUS_MASK_ALL);
  if (topic == NULL) {
    printf("***Error: failed to create topic\n");
  }

  // Create the reader
  struct DDS_DataReaderQos reader_qos = DDS_DataReaderQos_INITIALIZER;
  DDS_DataReader* reader;
  struct DDS_DataReaderListener reader_listener = DDS_DataReaderListener_INITIALIZER;
  retcode = DDS_Subscriber_get_default_datareader_qos(  subscriber, &reader_qos);
  if (retcode != DDS_RETCODE_OK) {
    printf("***Error: failed to get default datareader qos\n");
  }
  reader = DDS_Subscriber_create_datareader(subscriber,  DDS_Topic_as_topicdescription(topic),  &reader_qos,  NULL,  DDS_STATUS_MASK_ALL);
  if (reader == NULL) {
    printf("***Error: failed to create reader\n");
  }


  //Create the waitset and the condition for new messages
  DDS_WaitSet* waitset = DDS_WaitSet_new();
  //DDS_Condition* message_has_arrived = DDS_ReadCondition_as_condition(DDS_DataReader_create_readcondition(  reader,  DDS_NOT_READ_SAMPLE_STATE, DDS_NEW_VIEW_STATE, DDS_ANY_INSTANCE_STATE));
  DDS_Condition* status_condition = (DDS_Condition*)DDS_Entity_get_statuscondition((DDS_Entity*)reader);
  DDS_StatusMask statusmask = DDS_DATA_AVAILABLE_STATUS;

  retcode = DDS_StatusCondition_set_enabled_statuses((DDS_StatusCondition*)status_condition,  statusmask);
  if (retcode != DDS_RETCODE_OK) {
    printf("set_enabled_statuses error\n");
    return -1;
  }

  DDS_Condition* message_has_arrived = DDS_StatusCondition_as_condition (status_condition);
  retcode = DDS_WaitSet_attach_condition(waitset, message_has_arrived);
  if (retcode != DDS_RETCODE_OK) {
    printf("Could not attach condition to waitset...\n");
  }

  struct DDS_Duration_t timeout = { 3, 0}; // wait for 3s if there are no messages

  struct DDS_ConditionSeq active_conditions = DDS_SEQUENCE_INITIALIZER; // holder for active conditions

  DDS_WaitSet_get_conditions 	(waitset,	&active_conditions );

  int has_message_arrived = FALSE;

  //DDS_SampleStateMask   sample_state_mask = DDS_ANY_SAMPLE_STATE;
  //DDS_ViewStateMask     view_state_mask = DDS_ANY_VIEW_STATE;
  //DDS_InstanceStateMask instance_state_mask = DDS_ANY_INSTANCE_STATE;
  struct M12_CommandsSeq data_seq = DDS_SEQUENCE_INITIALIZER;
  struct DDS_SampleInfoSeq info_seq = DDS_SEQUENCE_INITIALIZER;

  // DDS INITIALIZATION END
  /* ---------------------------------------------------------------------------*/









/* ---------------------------------------------------------------------------*/
// CAN INITIALIZATION BEGIN
// Based on example found from:
// https://www.can-cia.org/fileadmin/resources/documents/proceedings/2012_kleine-budde.pdf

  struct ifreq ifr;
  struct sockaddr_can addr;
  struct can_frame frame;
  int s;

  memset(&ifr, 0x0, sizeof(ifr));
  memset(&addr, 0x0, sizeof(addr));
  memset(&frame, 0x0, sizeof(frame));

  s = socket(PF_CAN, SOCK_RAW, CAN_RAW);

  strcpy(ifr.ifr_name, "can0");
  if(ioctl(s, SIOCGIFINDEX, &ifr)){
    printf("ioctl went wrong...\n");
  }

  addr.can_ifindex = ifr.ifr_ifindex;
  addr.can_family = PF_CAN;
  if(bind(s, (struct sockaddr *)&addr, sizeof(addr))){
    printf("bind went wrong...\n");
  }

  /*
  frame.can_id = 0x23;
  frame.can_dlc = 8;
  frame.data[0] = 0x1;
  frame.data[1] = 0x2;
  frame.data[2] = 0x3;
  frame.data[3] = 0x4;
  frame.data[4] = 0x5;
  frame.data[5] = 0x6;
  frame.data[6] = 0x7;
  frame.data[7] = 0x8;
  */
  //write(s, &frame, sizeof(frame));

  frame.can_id = 0x707;
  frame.can_dlc = 1;
  frame.data[0] = 0x00;
  int how_much_written;
  how_much_written = write(s, &frame, sizeof(frame));


// CAN INITIALIZATION END
/* ---------------------------------------------------------------------------*/










  //----------------------------------------------------------------------------
  // MAIN LOOP STARTS HERE
  signal(SIGINT, interrupt_handler);
  struct timespec timer_start;
  struct timespec timer_end;

  int loop_counter = 0;
  mlockall(MCL_CURRENT|MCL_FUTURE);
  while( running ){

    clock_gettime(CLOCK_REALTIME, &timer_start);

    retcode = DDS_WaitSet_wait(waitset, &active_conditions, &timeout);




    if (retcode != DDS_RETCODE_OK) {
      printf("Problem with wait...\n");
    } else {

      printf("Data is available, accessing it...\n");
      M12_Commands instance_;
      retcode = M12_CommandsDataReader_take( 	(M12_CommandsDataReader * )reader, &data_seq, &info_seq, DDS_LENGTH_UNLIMITED, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);


      if (retcode != DDS_RETCODE_OK) {
        printf("Could not take the instance from the datareader...\n");
        return -1;
        // success
        //printf("Waitset ended...\n");
      }

      // CANopen heartbeat
      if(loop_counter == 0 || (loop_counter % 3 == 0)){
        frame.can_id = 0x707;
        frame.can_dlc = 1;
        frame.data[0] = 0x05;
        how_much_written = write(s, &frame, sizeof(frame));
        //printf("Wrote %i", how_much_written);
        //printf(" bytes to can\n");
      }

      struct M12_Commands* data = NULL;
      struct DDS_SampleInfo* info = NULL;
      int i;
      for(i = 0; i <  M12_CommandsSeq_get_length(&data_seq); ++i) {
        info = DDS_SampleInfoSeq_get_reference(&info_seq, i);
        data = M12_CommandsSeq_get_reference(&data_seq, i);

        printf("tilt_velocity: %i\n", data->tilt_velocity);
        printf("lift_velocity: %i\n", data->lift_velocity);
        printf("machine_velocity: %i\n", data->machine_velocity);
        printf("steering: %i\n", data->steering);
        printf("brakes_on: %i\n", data->brakes_on);

        // COB-ID of the command frame:
        frame.can_id = 0x187;
        frame.can_dlc = 8;
        //memcpy(&(frame.data),&(data->tilt_velocity), 2);
        //frame.data[0] = 0x1;
        //frame.data[1] = 0x2;



        frame.data[0] = data->tilt_velocity >> 8;
        frame.data[1] = data->tilt_velocity & 0x00ff;
        frame.data[2] = data->lift_velocity >> 8;
        frame.data[3] = data->lift_velocity & 0x00ff;

        //frame.data[4] = 0x0;

        if(data->brakes_on){
          frame.data[4] = 0x12;
        }
        else{
          frame.data[4] = 0x11;
        }


        frame.data[5] = data->machine_velocity;
        frame.data[6] = data->steering;
        frame.data[7] = 0x0; // reserved

        how_much_written = write(s, &frame, sizeof(frame));
        printf("Wrote %i", how_much_written);
        printf(" bytes to can\n");
      }

        /* Note that depending on the info->sample_state
        it is possible that data will be NULL
        */




        retcode = M12_CommandsDataReader_return_loan((M12_CommandsDataReader*)reader, &data_seq, &info_seq);
        if (retcode != DDS_RETCODE_OK) {
          printf("Could not return the loan...\n");
          return -1;
          // success
          //printf("Waitset ended...\n");
        }
        /*
        if (has_message_arrived) {
        printf("Received a M12_Commands message...\n");
        //---- DO STUFF HERE
        */


      }




      //----

      clock_gettime(CLOCK_REALTIME, &timer_end);
      //double seconds_elapsed = (double) timer_end.tv_sec -
      double seconds_elapsed = (timer_end.tv_sec - timer_start.tv_sec) * 1e6 + (timer_end.tv_nsec - timer_start.tv_nsec) / 1e3;    // in microseconds
      seconds_elapsed = seconds_elapsed/1e6;
      //double time_used = (double) (timer_start.tv_sec-timer_start)/CLOCKS_PER_SEC;
      printf("ELAPSED TIME IN MAIN LOOP: %f\n", seconds_elapsed);
      loop_counter = loop_counter + 1;
    }
    // MAIN LOOP ENDS HERE
    //----------------------------------------------------------------------------





    close(s);
    printf("Closing down the DDS participant...\n");


    if (participant != NULL) {
      retcode = DDS_DomainParticipant_delete_contained_entities(participant);
      if (retcode != DDS_RETCODE_OK) {
        fprintf(stderr, "delete_contained_entities error %d\n", retcode);
      }
      retcode = DDS_DomainParticipantFactory_delete_participant(
        DDS_TheParticipantFactory, participant);
        if (retcode != DDS_RETCODE_OK) {
          fprintf(stderr, "delete_participant error %d\n", retcode);
        }
      }
      printf("Exiting...\n");



    }
