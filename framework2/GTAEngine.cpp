#include <renderwure/engine/GTAEngine.hpp>
#include <renderwure/loaders/LoaderIPL.hpp>
#include <renderwure/loaders/LoaderIDE.hpp>

GTAEngine::GTAEngine(const std::string& path)
: itemCount(0), gameData(path), gameTime(0.f), randomEngine(rand())
{
	gameData.engine = this;
}

bool GTAEngine::load()
{
	gameData.load();
	
	// Loade all of the IDEs.
	for(std::map<std::string, std::string>::iterator it = gameData.ideLocations.begin();
		it != gameData.ideLocations.end();
		++it) {
		defineItems(it->second);
	}
	
	// Load the .zon IPLs since we want to have the zones loaded
	for(std::map<std::string, std::string>::iterator it = gameData.iplLocations.begin();
		it != gameData.iplLocations.end();
		++it) {
		loadZone(it->second);
		placeItems(it->second);
	}
	
	return true;
}

void GTAEngine::logInfo(const std::string& info)
{
	log.push_back({LogEntry::Info, gameTime, info});
}

void GTAEngine::logError(const std::string& error)
{
	log.push_back({LogEntry::Error, gameTime, error});
}

void GTAEngine::logWarning(const std::string& warning)
{
	log.push_back({LogEntry::Warning, gameTime, warning});
}

bool GTAEngine::defineItems(const std::string& name)
{
	auto i = gameData.ideLocations.find(name);
	std::string path = name;
	
	if( i != gameData.ideLocations.end()) {
		path = i->second;
	}
	else {
		std::cout << "IDE not pre-listed" << std::endl;
	}
	
	LoaderIDE idel;
	
	if(idel.load(path)) {
		for( size_t o = 0; o < idel.OBJSs.size(); ++o) {
			objectTypes.insert({
				idel.OBJSs[o].ID, 
				std::shared_ptr<LoaderIDE::OBJS_t>(new LoaderIDE::OBJS_t(idel.OBJSs[o]))
			});
		}
		
		for( size_t v = 0; v < idel.CARSs.size(); ++v) {
			std::cout << "Vehicle ID " << idel.CARSs[v].ID << ": " << idel.CARSs[v].gameName << std::endl;
			vehicleTypes.insert({
				idel.CARSs[v].ID,
				std::shared_ptr<LoaderIDE::CARS_t>(new LoaderIDE::CARS_t(idel.CARSs[v]))
			});
		}
	}
	else {
		std::cerr << "Failed to load IDE " << path << std::endl;
	}
	
	return false;
}

bool GTAEngine::placeItems(const std::string& name)
{
	auto i = gameData.iplLocations.find(name);
	std::string path = name;
	
	if(i != gameData.iplLocations.end())
	{
		path = i->second;
	}
	else
	{
		std::cout << "IPL not pre-listed" << std::endl;
	}
	
	LoaderIPL ipll;

	if(ipll.load(path))
	{
		// Find the object.
		for( size_t i = 0; i < ipll.m_instances.size(); ++i) {
			LoaderIPLInstance& inst = ipll.m_instances[i];
			auto oi = objectTypes.find(inst.id);
			if( oi != objectTypes.end()) {
				// Make sure the DFF and TXD are loaded
				if(! oi->second->modelName.empty()) {
					gameData.loadDFF(oi->second->modelName + ".dff");
				}
				if(! oi->second->textureName.empty()) {
					gameData.loadTXD(oi->second->textureName + ".txd");
				}
				
				objectInstances.push_back({ inst, oi->second });
			}
			else {
				std::cerr << "No object for instance " << inst.id << " (" << path << ")" << std::endl;
			}
		}
		itemCentroid += ipll.centroid;
		itemCount += ipll.m_instances.size();
		return true;
	}
	else
	{
		std::cerr << "Failed to load IPL: " << path << std::endl;
		return false;
	}
	
	return false;
}

bool GTAEngine::loadZone(const std::string& path)
{
	LoaderIPL ipll;

	if( ipll.load(path)) {
		if( ipll.zones.size() > 0) {
			zones.insert(zones.begin(), ipll.zones.begin(), ipll.zones.end());
			std::cout << "Loaded " << ipll.zones.size() << " zones" << std::endl;
			return true;
		}
	}
	else {
		std::cerr << "Failed to load IPL " << path << std::endl;
	}
	
	return false;
}

void GTAEngine::createVehicle(const uint16_t id, const glm::vec3& pos)
{
	auto vti = vehicleTypes.find(id);
	if(vti != vehicleTypes.end()) {
		std::cout << "Creating Vehicle ID " << id << " (" << vti->second->gameName << ")" << std::endl;
		
		if(! vti->second->modelName.empty()) {
			gameData.loadDFF(vti->second->modelName + ".dff");
		}
		if(! vti->second->textureName.empty()) {
			gameData.loadTXD(vti->second->textureName + ".txd");
		}
		
		glm::vec3 prim = glm::vec3(1.f), sec = glm::vec3(0.5f);
		auto palit = gameData.vehiclePalettes.find(vti->second->modelName); // modelname is conveniently lowercase (usually)
		if(palit != gameData.vehiclePalettes.end() && palit->second.size() > 0 ) {
			 std::uniform_int_distribution<int> uniform(0, palit->second.size()-1);
			 int set = uniform(randomEngine);
			 prim = gameData.vehicleColours[palit->second[set].first];
			 sec = gameData.vehicleColours[palit->second[set].second];
		}
		else {
			logWarning("No colour palette for vehicle " + vti->second->modelName);
		}
		
		auto wi = objectTypes.find(vti->second->wheelModelID);
		if( wi != objectTypes.end()) {
			if(! wi->second->textureName.empty()) {
				gameData.loadTXD(wi->second->textureName + ".txd");
			}
		}
		
		std::unique_ptr<Model> &model = gameData.models[vti->second->modelName];
		if(model) {
			if( vti->second->wheelPositions.size() == 0 ) {
				for( size_t f = 0; f < model->frames.size(); ++f) {
					if( model->frameNames.size() > f) {
						std::string& name = model->frameNames[f];
						if( name.substr(0, 5) == "wheel" ) {
							vti->second->wheelPositions.push_back(model->frames[f].position);
						}
					}
				}
			}
		}
		
		vehicleInstances.push_back({ pos, vti->second, prim, sec });
	}
}
