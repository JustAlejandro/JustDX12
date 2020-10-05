#include "ModelLoading\ModelLoader.h"
#include "ModelLoading\Model.h"

ModelLoader::ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice)
	: TaskQueueThread(d3dDevice, D3D12_COMMAND_LIST_TYPE_COPY) {

}

Model* ModelLoader::loadModel(std::string name, std::string dir) {
	Model* loading = new Model(name, dir);
	enqueue(new ModelLoadTask(this, loading));
	return loading;
}
