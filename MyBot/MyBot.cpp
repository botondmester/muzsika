#include "MyBot.h"
#include <dpp/dpp.h>
#include <mpg123.h>

struct Song {
	std::string url = "";
	std::string name = "";
};

struct GuildStatus {
	bool loop = false;
	Song current = { "","" };
	std::queue<Song> queue;
};

std::vector<uint8_t> MP3toPCM(std::vector<uint8_t> mp3) {
	std::vector<uint8_t> pcmdata;

	int err = 0;
	unsigned char* buffer;
	size_t buffer_size, done;
	int channels, encoding;
	long rate;

	mpg123_handle* mh = mpg123_new(NULL, &err);
	mpg123_param(mh, MPG123_FORCE_RATE, 48000, 48000.0);

	buffer_size = mpg123_outblock(mh);
	buffer = new unsigned char[buffer_size];

	mpg123_open_feed(mh);
	if (mpg123_feed(mh, mp3.data(), mp3.size()) != MPG123_OK) {
		throw new std::runtime_error("Failed to feed transcoder");
	};
	if(mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
		throw new std::runtime_error("Failed to get format");
	};

	for (size_t totalBytes = 0; mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK; ) {
		for (size_t i = 0; i < buffer_size; i++) {
			pcmdata.push_back(buffer[i]);
		}
		totalBytes += done;
	}
	delete[] buffer;
	mpg123_close(mh);
	mpg123_delete(mh);
	return pcmdata;
}

void sendMusic(dpp::discord_voice_client *vc, std::vector<uint8_t> &pcmdata) {
	size_t to_send = pcmdata.size();
	uint8_t* head = pcmdata.data();
	while (to_send > 0) {
		if (to_send >= dpp::send_audio_raw_max_length) {
			vc->send_audio_raw((uint16_t*)head, dpp::send_audio_raw_max_length);
			head += dpp::send_audio_raw_max_length;
			to_send -= dpp::send_audio_raw_max_length;
		}
		else {
			vc->send_audio_raw((uint16_t*)head, to_send);
			break;
		}
	}
	vc->insert_marker();
}

std::map<dpp::snowflake, GuildStatus> guildStatuses;

int main()
{
	mpg123_init();

	std::ifstream file;
	std::stringstream buffer;
	std::cout << std::filesystem::current_path();
	file.open("token.txt", std::ios::in | std::ios::binary);
	if (file.is_open()) {
		buffer << file.rdbuf();
	}
	else return 2;
	file.close();
	std::string BOT_TOKEN = buffer.str();

	/* Create bot cluster */
	dpp::cluster bot(BOT_TOKEN);

	/* Output simple log messages to stdout */
	bot.on_log(dpp::utility::cout_logger());

	/* Register slash command here in on_ready */
	bot.on_ready([&bot](const dpp::ready_t& event) {
		/* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
#ifndef _DEBUG
		if (dpp::run_once<struct register_bot_commands>()) {
#endif // _DEBUG
			dpp::slashcommand mp3 = { "mp3", "Play an mp3.", bot.me.id };
			mp3.add_option(dpp::command_option(dpp::co_attachment, "file", "Select an mp3 file", true));
			dpp::slashcommand loop = { "loop", "Toggles looping", bot.me.id };
			loop.add_option(dpp::command_option(dpp::co_boolean, "toggle", "to loop or not to loop", true));
			std::vector<dpp::slashcommand> commands{
				{ "ping", "Ping pong!", bot.me.id },
				{ "join", "Join your voice channel.", bot.me.id },
				mp3,
				loop,
				{ "pause", "Pause the player", bot.me.id },
				{ "resume", "Resume the player", bot.me.id },
				{ "skip", "Skip current song", bot.me.id },
				{ "queue", "What's in the queue", bot.me.id }
			};

			bot.global_bulk_command_create(commands);
#ifndef _DEBUG
		}
#endif // _DEBUG
	});

	/* Handle slash command with the most recent addition to D++ features, coroutines! */
	bot.on_slashcommand([&bot](const dpp::slashcommand_t& event) -> dpp::task<void> {
		if (event.command.get_command_name() == "ping") {
			event.reply("Pong!");
		}
		else if (event.command.get_command_name() == "join") {
			/* Get the guild */
			dpp::guild* g = dpp::find_guild(event.command.guild_id);

			/* Attempt to connect to a voice channel, returns false if we fail to connect. */
			if (!g->connect_member_voice(event.command.get_issuing_user().id)) {
				event.reply("You don't seem to be in a voice channel!");
				co_return;
			}

			/* Tell the user we joined their channel. */
			event.reply("Joined your channel!");
		}
		else if (event.command.get_command_name() == "pause") {
			dpp::voiceconn* v = event.from->get_voice(event.command.guild_id);

			if (!v || !v->voiceclient || !v->voiceclient->is_ready()) {
				event.reply("There was an issue with getting the voice channel. Make sure I'm in a voice channel!");
				co_return;
			}
			v->voiceclient->pause_audio(true);
			event.reply(":pause_button: Paused!");
		}
		else if (event.command.get_command_name() == "resume") {
			dpp::voiceconn* v = event.from->get_voice(event.command.guild_id);

			if (!v || !v->voiceclient || !v->voiceclient->is_ready()) {
				event.reply("There was an issue with getting the voice channel. Make sure I'm in a voice channel!");
				co_return;
			}
			v->voiceclient->pause_audio(false);
			event.reply(":arrow_forward: Resumed!");
		}
		else if (event.command.get_command_name() == "skip") {
			dpp::voiceconn* v = event.from->get_voice(event.command.guild_id);
			if (!v || !v->voiceclient || !v->voiceclient->is_ready()) {
				event.reply("There was an issue with getting the voice channel. Make sure I'm in a voice channel!");
				co_return;
			}
			if (guildStatuses[event.command.guild_id].current.url != "") {
				event.reply("Skipped from `" + guildStatuses[event.command.guild_id].current.name + "` to the next song");
				v->voiceclient->skip_to_next_marker();
				v->voiceclient->send_silence(60);
				v->voiceclient->insert_marker();
				co_return;
			}
			event.reply("There's no song playing");
		}
		else if (event.command.get_command_name() == "loop") {
			dpp::voiceconn* v = event.from->get_voice(event.command.guild_id);

			if (!v || !v->voiceclient || !v->voiceclient->is_ready()) {
				event.reply("There was an issue with getting the voice channel. Make sure I'm in a voice channel!");
				co_return;
			}
			bool loop = std::get<bool>(event.get_parameter("toggle"));
			guildStatuses[event.command.guild_id].loop = loop;
			event.reply(loop ? "Toggled looping on" : "Toggled looping off");
		}
		else if (event.command.get_command_name() == "queue") {
			dpp::voiceconn* v = event.from->get_voice(event.command.guild_id);

			if (!v || !v->voiceclient || !v->voiceclient->is_ready()) {
				event.reply("There was an issue with getting the voice channel. Make sure I'm in a voice channel!");
				co_return;
			}

			std::string list = "";

			for (int i = 0; i < guildStatuses[event.command.guild_id].queue.size(); i++) {
				list += (std::to_string(i) + ": `" + guildStatuses[event.command.guild_id].queue.front().name + "`\n");
				guildStatuses[event.command.guild_id].queue.push(guildStatuses[event.command.guild_id].queue.front());
				guildStatuses[event.command.guild_id].queue.pop();
			}
			event.reply("This is the current queue:\n" + list);
		}
		else if (event.command.get_command_name() == "mp3") {
			dpp::voiceconn* v = event.from->get_voice(event.command.guild_id);

			if (!v || !v->voiceclient || !v->voiceclient->is_ready()) {
				event.reply("There was an issue with getting the voice channel. Make sure I'm in a voice channel!");
				co_return;
			}
			dpp::snowflake file_id = std::get<dpp::snowflake>(event.get_parameter("file"));
			dpp::attachment att = event.command.get_resolved_attachment(file_id);
			if (att.content_type != "audio/mpeg") {
				event.reply(":moyai: This is not an mp3 file, you idiot");
				co_return;
			}
			guildStatuses[event.command.guild_id].queue.push({ att.url, att.filename });
			if (guildStatuses[event.command.guild_id].current.url == "") {
				v->voiceclient->send_silence(60);
				v->voiceclient->insert_marker();
			}
			event.reply("Added `" + att.filename + "` to the queue");
		}
		co_return;
	});

	bot.on_voice_track_marker([&bot](const dpp::voice_track_marker_t& marker)->dpp::task<void> {
		dpp::discord_voice_client* vc = marker.voice_client;
		GuildStatus &s = guildStatuses[vc->server_id];
		if (s.loop && s.current.url != "") {
			s.queue.push(s.current);
		}
		if (s.queue.empty()) {
			s.current = { "", "" };
			co_return;
		}
		s.current = s.queue.front();
		s.queue.pop();
		// download mpeg-2 file from attachment url
		dpp::http_request_completion_t http = co_await bot.co_request(s.current.url, dpp::m_get);
		std::vector<uint8_t> mp3Data(http.body.begin(), http.body.end());
		std::vector<uint8_t> pcmData = MP3toPCM(mp3Data);
		sendMusic(vc, pcmData);
	});

	/* Start the bot */
	bot.start(dpp::st_wait);

	return 0;
}
