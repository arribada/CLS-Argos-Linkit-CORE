#include <vector>
#include "dte_protocol.hpp"
#include "config_store.hpp"

enum class DTEAction {
	NONE,
	RESET,
	SECUR
};


class DTEHandler {
private:
	enum class DTEError {
	};

	static std::string PARML_REQ(int error_code) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARML_RESP, error_code);
		}

		// Build up a list of all implemented parameters
		std::vector<ParamID> params;
		for (unsigned int i = 0; i < sizeof(param_map)/sizeof(BaseMap); i++) {
			if (param_map[i].is_implemented) {
				params.push_back(static_cast<ParamID>(i));
			}
		}

		return DTEEncoder::encode(DTECommand::PARML_RESP, params);
	}

public:
	static DTEAction handle_dte_message(ConfigurationStore* store, std::string& req, std::string& resp) {
		DTECommand command;
		std::vector<ParamID> params;
		std::vector<ParamValue> param_values;
		std::vector<BaseType> arg_list;
		unsigned int error_code;
		DTEAction action = DTEAction::NONE;

		try {
			if (!DTEDecoder::decode(req, command, error_code, arg_list, params, param_values))
				return DTEAction::NONE;
		} catch (int e) {
			// TODO: generic error response handler
			error_code = 0;
		}

		switch(command) {
		case DTECommand::PARML_REQ:
			resp = PARML_REQ(error_code);
			break;
		default:
			break;
		}

		return action;
	}
};
