// Copyright (C) by Josh Blum. See LICENSE.txt for licensing information.

#include <gras_impl/block_actor.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

using namespace gras;


void BlockActor::handle_top_active(
    const TopActiveMessage &message,
    const Theron::Address from
){
    MESSAGE_TRACER();

    if (this->block_state != BLOCK_STATE_LIVE)
    {
        this->block_ptr->notify_active();
        this->stats.start_time = time_now();
    }
    this->block_state = BLOCK_STATE_LIVE;

    this->Send(0, from); //ACK

    this->task_kicker();
}

void BlockActor::handle_top_inert(
    const TopInertMessage &,
    const Theron::Address from
){
    MESSAGE_TRACER();

    this->mark_done();

    this->Send(0, from); //ACK
}

void BlockActor::handle_top_token(
    const TopTokenMessage &message,
    const Theron::Address from
){
    MESSAGE_TRACER();

    //create input tokens and send allocation hints
    for (size_t i = 0; i < this->get_num_inputs(); i++)
    {
        this->input_tokens[i] = Token::make();
        this->inputs_done.reset(i);
        OutputTokenMessage token_msg;
        token_msg.token = this->input_tokens[i];
        this->post_upstream(i, token_msg);

        //TODO, schedule this message as a pre-allocation message
        //tell the upstream about the input requirements
        OutputHintMessage output_hints;
        output_hints.reserve_bytes = this->input_configs[i].reserve_items*this->input_configs[i].item_size;
        output_hints.token = this->input_tokens[i];
        this->post_upstream(i, output_hints);

    }

    //create output token
    for (size_t i = 0; i < this->get_num_outputs(); i++)
    {
        this->output_tokens[i] = Token::make();
        this->outputs_done.reset(i);
        InputTokenMessage token_msg;
        token_msg.token = this->output_tokens[i];
        this->post_downstream(i, token_msg);
    }

    //store a token to the top level topology
    this->token_pool.insert(message.token);

    this->Send(0, from); //ACK
}

void BlockActor::handle_top_config(
    const GlobalBlockConfig &message,
    const Theron::Address from
){
    MESSAGE_TRACER();

    //overwrite with global config only if maxium_items is not set (zero)
    for (size_t i = 0; i < this->output_configs.size(); i++)
    {
        if (this->output_configs[i].maximum_items == 0)
        {
            this->output_configs[i].maximum_items = message.maximum_output_items;
        }
    }

    //overwrite with global node affinity setting for buffers if not set
    if (this->buffer_affinity == -1)
    {
        this->buffer_affinity = message.buffer_affinity;
    }

    this->Send(0, from); //ACK
}

void BlockActor::handle_top_thread_group(
    const SharedThreadGroup &message,
    const Theron::Address from
){
    MESSAGE_TRACER();

    //store the topology's thread group
    //erase any potentially old lingering threads
    //spawn a new thread if this block is a source
    this->thread_group = message;
    this->interruptible_thread.reset(); //erase old one
    if (this->interruptible_work)
    {
        this->interruptible_thread = boost::make_shared<InterruptibleThread>(
            this->thread_group, boost::bind(&BlockActor::task_work, this)
        );
    }

    this->Send(0, from); //ACK
}

void BlockActor::handle_self_kick(
    const SelfKickMessage &,
    const Theron::Address
){
    MESSAGE_TRACER();
    this->task_main();
}

void BlockActor::handle_get_stats(
    const GetStatsMessage &,
    const Theron::Address from
){
    MESSAGE_TRACER();

    //instantaneous states we update here,
    //and not interleaved with the rest of the code
    const size_t num_inputs = this->get_num_inputs();
    this->stats.items_enqueued.resize(num_inputs);
    this->stats.tags_enqueued.resize(num_inputs);
    this->stats.msgs_enqueued.resize(num_inputs);
    for (size_t i = 0; i < num_inputs; i++)
    {
        this->stats.items_enqueued[i] = this->input_queues.get_items_enqueued(i);
        this->stats.tags_enqueued[i] = this->input_tags[i].size();
        this->stats.msgs_enqueued[i] = this->input_msgs[i].size();
    }

    //create the message reply object
    GetStatsMessage message;
    message.block_id = this->block_ptr->to_string();
    message.stats = this->stats;
    message.stats_time = time_now();

    this->Send(message, from); //ACK

    //work could have been skipped by a high prio msg
    //forcefully kick the task to recheck in a new call
    this->Send(SelfKickMessage(), this->GetAddress());
}
