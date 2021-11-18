#define PACKAGE_GPC_TOOL 1

#ifdef PACKAGE_GPC_TOOL
#include <stdio.h>
#include <fc/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <fc/exception/exception.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <uvm/uvm_api.h>
#include <fc/io/json.hpp>
#include <fc/io/varint.hpp>
#include <boost/uuid/sha1.hpp>
#include <vector>
#include <map>
using namespace std;

struct CodeInfo {
	std::vector<std::string> api;
	std::vector<std::string> offline_api;
	std::vector<std::string> event;
	std::map<std::string, uint32_t> storage_properties_types;

	std::map<std::string, std::vector<uint32_t>> api_args_types;
};

UvmModuleByteStream::UvmModuleByteStream()
{
	is_bytes = false;
	contract_level = CONTRACT_LEVEL_TEMP;
	contract_state = CONTRACT_STATE_VALID;
}

UvmModuleByteStream::~UvmModuleByteStream()
{
	contract_apis.clear();
	offline_apis.clear();
	contract_emit_events.clear();
}

//FC_REFLECT(::CodeInfo,
//(api)
//(offline_api)
//(event)
//(storage_properties_types)
//(api_args_types)
//)

struct GpcBuffer
{
	std::vector<unsigned char> data;
	size_t pos = 0;
	bool eof() const { return pos >= data.size(); }
	size_t size() const { return data.size(); }
};

int gpcread(void* ptr, size_t element_size, size_t count, GpcBuffer* gpc_buffer)
{
	if (!ptr)
		return 0;
	if (gpc_buffer->eof() || ((gpc_buffer->data.size() - gpc_buffer->pos) < element_size))
		return 0;
	if (element_size*count <= (gpc_buffer->data.size() - gpc_buffer->pos))
	{
		memcpy(ptr, gpc_buffer->data.data() + gpc_buffer->pos, element_size*count);
		gpc_buffer->pos += element_size*count;
		return count;
	}
	size_t available_count = (gpc_buffer->data.size() - gpc_buffer->pos) / element_size;
	memcpy(ptr, gpc_buffer->data.data() + gpc_buffer->pos, element_size * available_count);
	gpc_buffer->pos += element_size * available_count;
	return available_count;
}
int gpcwrite(const void* ptr, size_t element_size, size_t count, GpcBuffer* gpc_buffer)
{
	if (!ptr)
		return 0;
	if (element_size * count > (gpc_buffer->data.size() - gpc_buffer->pos))
	{
		gpc_buffer->data.resize(gpc_buffer->pos + element_size * count);
	}
	memcpy(gpc_buffer->data.data() + gpc_buffer->pos, ptr, element_size*count);
	gpc_buffer->pos += element_size * count;
	return count;
}
int common_fread_int(GpcBuffer* fp, int* dst_int)
{
	int ret;
	unsigned char uc4, uc3, uc2, uc1;
	ret = (int)gpcread(&uc4, sizeof(unsigned char), 1, fp);
	if (ret != 1)
		return ret;
	ret = (int)gpcread(&uc3, sizeof(unsigned char), 1, fp);
	if (ret != 1)
		return ret;
	ret = (int)gpcread(&uc2, sizeof(unsigned char), 1, fp);
	if (ret != 1)
		return ret;
	ret = (int)gpcread(&uc1, sizeof(unsigned char), 1, fp);
	if (ret != 1)
		return ret;
	*dst_int = (uc4 << 24) + (uc3 << 16) + (uc2 << 8) + uc1;
	return 1;
}
int common_fread_octets(GpcBuffer* fp, void* dst_stream, int len)
{
	return (int)gpcread(dst_stream, len, 1, fp);
}
int common_fwrite_stream(GpcBuffer* fp, const void* src_stream, int len)
{
	return (int)gpcwrite(src_stream, len, 1, fp);
}
int common_fwrite_int(GpcBuffer* fp, const int* src_int)
{
	int ret;
	unsigned char uc4, uc3, uc2, uc1;
	uc4 = ((*src_int) & 0xFF000000) >> 24;
	uc3 = ((*src_int) & 0x00FF0000) >> 16;
	uc2 = ((*src_int) & 0x0000FF00) >> 8;
	uc1 = (*src_int) & 0x000000FF;
	ret = (int)gpcwrite(&uc4, sizeof(unsigned char), 1, fp);
	if (ret != 1)
		return ret;
	ret = (int)gpcwrite(&uc3, sizeof(unsigned char), 1, fp);
	if (ret != 1)
		return ret;
	ret = (int)gpcwrite(&uc2, sizeof(unsigned char), 1, fp);
	if (ret != 1)
		return ret;
	ret = (int)gpcwrite(&uc1, sizeof(unsigned char), 1, fp);
	if (ret != 1)
		return ret;
	return 1;
}

int save_code_to_file(const std::string& name, UvmModuleByteStream *stream, char* err_msg)
{
	boost::uuids::detail::sha1 sha;
	unsigned int digest[5];
	UvmModuleByteStream* p_new_stream = new UvmModuleByteStream();
	if (nullptr == p_new_stream)
	{
		strcpy(err_msg, "malloc UvmModuleByteStream fail");
		return -1;
	}
	p_new_stream->is_bytes = stream->is_bytes;
	p_new_stream->buff = stream->buff;
	for (int i = 0; i < stream->contract_apis.size(); ++i)
	{
		int new_flag = 1;
		for (int j = 0; j < stream->offline_apis.size(); ++j)
		{
			if (stream->contract_apis[i] == stream->offline_apis[j])
			{
				new_flag = 0;
				continue;
			}
		}
		if (new_flag)
		{
			p_new_stream->contract_apis.push_back(stream->contract_apis[i]);
		}
	}
	p_new_stream->offline_apis = stream->offline_apis;
	p_new_stream->contract_emit_events = stream->contract_emit_events;
	p_new_stream->contract_storage_properties = stream->contract_storage_properties;
	p_new_stream->contract_id = stream->contract_id;
	p_new_stream->contract_name = stream->contract_name;
	p_new_stream->contract_level = stream->contract_level;
	p_new_stream->contract_state = stream->contract_state;
	FILE *f = fopen(name.c_str(), "wb");
	if (nullptr == f)
	{
		delete (p_new_stream);
		strcpy(err_msg, strerror(errno));
		return -1;
	}
	GpcBuffer gpc_buffer;
	gpc_buffer.pos = 0;
	sha.process_bytes(p_new_stream->buff.data(), p_new_stream->buff.size());
	sha.get_digest(digest);
	for (int i = 0; i < 5; ++i)
		common_fwrite_int(&gpc_buffer, (int*)&digest[i]);
	int p_new_stream_buf_size = (int)p_new_stream->buff.size();
	common_fwrite_int(&gpc_buffer, &p_new_stream_buf_size);
	p_new_stream->buff.resize(p_new_stream_buf_size);
	common_fwrite_stream(&gpc_buffer, p_new_stream->buff.data(), p_new_stream->buff.size());
	int contract_apis_count = (int)p_new_stream->contract_apis.size();
	common_fwrite_int(&gpc_buffer, &contract_apis_count);
	for (int i = 0; i < contract_apis_count; ++i)
	{
		int api_len = p_new_stream->contract_apis[i].length();
		common_fwrite_int(&gpc_buffer, &api_len);
		common_fwrite_stream(&gpc_buffer, p_new_stream->contract_apis[i].c_str(), api_len);
	}
	int offline_apis_count = (int)p_new_stream->offline_apis.size();
	common_fwrite_int(&gpc_buffer, &offline_apis_count);
	for (int i = 0; i < offline_apis_count; ++i)
	{
		int offline_api_len = p_new_stream->offline_apis[i].length();
		common_fwrite_int(&gpc_buffer, &offline_api_len);
		common_fwrite_stream(&gpc_buffer, p_new_stream->offline_apis[i].c_str(), offline_api_len);
	}
	int contract_emit_events_count = p_new_stream->contract_emit_events.size();
	common_fwrite_int(&gpc_buffer, &contract_emit_events_count);
	for (int i = 0; i < contract_emit_events_count; ++i)
	{
		int event_len = p_new_stream->contract_emit_events[i].length();
		common_fwrite_int(&gpc_buffer, &event_len);
		common_fwrite_stream(&gpc_buffer, p_new_stream->contract_emit_events[i].c_str(), event_len);
	}
	int contract_storage_properties_count = p_new_stream->contract_storage_properties.size();
	common_fwrite_int(&gpc_buffer, &contract_storage_properties_count);
	for (const auto& storage_info : p_new_stream->contract_storage_properties)
	{
		int storage_len = storage_info.first.length();
		common_fwrite_int(&gpc_buffer, &storage_len);
		common_fwrite_stream(&gpc_buffer, storage_info.first.c_str(), storage_len);
		int storage_type = storage_info.second;
		common_fwrite_int(&gpc_buffer, &storage_type);
	}
	fwrite(gpc_buffer.data.data(), gpc_buffer.pos, 1, f);
	fclose(f);
	delete (p_new_stream);
	return 0;
}

int main(int argc,char** argv)
{
	try{
	if (argc < 3)
	{
		perror("No enough params\n");
		return 0;
	}
	boost::filesystem::path code_file(argv[1]);
	boost::filesystem::path codeinfo_file(argv[2]);
	if (!boost::filesystem::exists(code_file) || !boost::filesystem::exists(codeinfo_file)) {
		perror("the file not exist!\n");
		return 1;
	}
	string out_filename = code_file.string();

	size_t pos;

	pos = code_file.string().find_last_of('.');
	if ((pos != string::npos))
	{
		out_filename = code_file.string().substr(0, pos) + ".gpc";
	}
	else
	{
		perror("contract source file name should end with .lua or .uvm\n");
		return 1;
	}
	std::ifstream file(code_file.string().c_str(), std::ifstream::binary);
	file.seekg(0, file.end);
	auto length = file.tellg();
	UvmModuleByteStream* p_new_stream = new UvmModuleByteStream();
	if (p_new_stream == nullptr)
	{
		perror("Create UvmModuleByteStream Failed\n");
		return 1;
	}
		
	p_new_stream->buff.resize(length);
	file.seekg(0, file.beg);
	file.read(p_new_stream->buff.data(), length);
	file.close();

	std::ifstream meta_file(codeinfo_file.string().c_str(), std::ifstream::binary);
	meta_file.seekg(0, meta_file.end);
	auto metafile_length = meta_file.tellg();
	std::vector<char> metafile_chars(metafile_length);
	meta_file.seekg(0, meta_file.beg);
	meta_file.read(metafile_chars.data(), metafile_length);
	meta_file.close();
	auto metajson = fc::json::from_string(std::string(metafile_chars.begin(), metafile_chars.end()), fc::json::legacy_parser).as<fc::mutable_variant_object>();
	const auto& info_apis = metajson["api"].as<fc::variants>();
	for (size_t i = 0; i < info_apis.size(); i++)
	{
		p_new_stream->contract_apis.push_back(info_apis[i].as_string());
	}
	const auto& info_events = metajson["event"].as<fc::variants>();
	for (size_t i = 0; i < info_events.size(); i++)
	{
		p_new_stream->contract_emit_events.push_back(info_events[i].as_string());
	}
	const auto& info_offline_apis = metajson["offline_api"].as<fc::variants>();
	for (size_t i = 0; i < info_offline_apis.size(); i++)
	{
		p_new_stream->offline_apis.push_back(info_offline_apis[i].as_string());
	}
	const auto& info_api_args_types = metajson["api_args_types"].as<fc::variants>();
	for (auto it = info_api_args_types.begin(); it != info_api_args_types.end();it++)
	{
		const auto& items = it->as<fc::variants>();
		const auto& item_values = items[1].as<fc::variants>();
		vector<UvmTypeInfoEnum> vec;
		for (auto it2 = item_values.begin(); it2 != item_values.end(); it2++) {
			fc::unsigned_int type_it = it2->as_int64();
			vec.push_back((UvmTypeInfoEnum)type_it);
		}
		p_new_stream->contract_api_arg_types.insert(std::make_pair(items[0].as_string(), vec));
	}
	const auto& info_storage_properties = metajson["storage_properties_types"].as<fc::variants>();
	for (auto it = info_storage_properties.begin(); it != info_storage_properties.end(); it++)
	{
		const auto& items = it->as<fc::variants>();
		auto value = items[1].as_int64();
		p_new_stream->contract_storage_properties[items[0].as_string()] = (fc::unsigned_int) value;
	}
	
	if (save_code_to_file(out_filename, p_new_stream, nullptr) < 0)
	{
		delete p_new_stream;
		p_new_stream = nullptr;
		perror("save bytecode to gpcfile failed");
		return 1;
	}
	if (p_new_stream)
		delete p_new_stream;
	printf("%s\n", out_filename.c_str()) ;
	return 0;
	}
	catch (std::exception &e)
	{
		printf(e.what());
		printf("Unknown Error\n");
	}
}
#endif