#include <stdio.h>
//#include "subscribe_and_write2CAN.hpp"
#include "M12_Commands.h"
#include "M12_CommandsPlugin.h"
#include "M12_CommandsSupport.h"
#include "ndds/ndds_c.h"

#define TRUE 1
#define FALSE 0

int main(){

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


  /* initialize participant_qos with default values */
  retcode = DDS_DomainParticipantFactory_get_default_participant_qos(factory, &participant_qos);
  if (retcode != DDS_RETCODE_OK) {
    printf("***Error: failed to get default participant qos\n");
  }
  if (!DDS_StringSeq_from_array(&participant_qos.discovery.initial_peers,NDDS_DISCOVERY_INITIAL_PEERS,  NDDS_DISCOVERY_INITIAL_PEERS_LENGTH)) {
    printf("***Error: failed to set discovery.initial_peers qos\n");
  }

  /* Create the participant */
  int domain_id = 0;
  participant =
  DDS_DomainParticipantFactory_create_participant(factory,domain_id,  &participant_qos,  NULL,DDS_STATUS_MASK_ALL);
  if (participant == NULL) {
    printf("***Error: failed to create domain participant\n");
    return -1;
  }



  /* Create the subscriber */
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


  /* Register and create the topic */
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

  /* Create the reader */
  struct DDS_DataReaderQos reader_qos = DDS_DataReaderQos_INITIALIZER;
  DDS_DataReader* reader;
  struct DDS_DataReaderListener reader_listener =
  DDS_DataReaderListener_INITIALIZER;
  retcode = DDS_Subscriber_get_default_datareader_qos(  subscriber, &reader_qos);
  if (retcode != DDS_RETCODE_OK) {
    printf("***Error: failed to get default datareader qos\n");
  }
  reader = DDS_Subscriber_create_datareader(subscriber,  DDS_Topic_as_topicdescription(topic),  &reader_qos,  NULL,  DDS_STATUS_MASK_ALL);
  if (reader == NULL) {
    printf("***Error: failed to create reader\n");
  }


  /* Create the waitset and the condition for new messages */
  DDS_WaitSet* waitset = DDS_WaitSet_new();


  /* DDS_NOT_READ_SAMPLE_STATE */
  DDS_Condition* message_has_arrived = DDS_ReadCondition_as_condition(DDS_DataReader_create_readcondition(  reader,  DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE));

  retcode = DDS_WaitSet_attach_condition(waitset, message_has_arrived);
  if (retcode != DDS_RETCODE_OK) {
    printf("Could not attach condition to waitset...\n");
  }

  struct DDS_Duration_t timeout = { 8*60*60, 1000000}; /* 8h */
  struct DDS_ConditionSeq active_conditions = DDS_SEQUENCE_INITIALIZER; /* holder for active conditions */
  int has_message_arrived = FALSE;

  //----------------------------------------------------------------------------
  // MAIN LOOP STARTS HERE
  retcode = DDS_WaitSet_wait(waitset, &active_conditions, &timeout);
  if (retcode != DDS_RETCODE_OK) {
    /* ... check for cause of failure */
  } else {
    /* success */
    if (DDS_ConditionSeq_get_length(&active_conditions) == 0) {
      /* timeout! */
      printf("Wait timed out!! None of the conditions was triggered.\n");
    } else {
      /* check if "cond1" or "cond2" are triggered: */
      int i;
      for(i = 0; i < DDS_ConditionSeq_get_length(&active_conditions); ++i) {
        if (DDS_ConditionSeq_get(&active_conditions, i) == message_has_arrived) {
          printf("Message has arrived!\n");
          has_message_arrived = TRUE;
        }
        if (has_message_arrived) {
          break;
        }
      }
    }
  }
  if (has_message_arrived) {
    printf("Received a M12_Commands message...\n");
  }
  // MAIN LOOP ENDS HERE
  //----------------------------------------------------------------------------






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
