#pragma once

#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/TaskResultTypes.h"
#include "MaaUtils/NoWarningCVMat.hpp"

#include "Common/Conf.h"

MAA_NS_BEGIN

class RuntimeCache
{
public:
    std::optional<MaaNodeId> get_latest_node(const std::string& name) const;
    void set_latest_node(const std::string& name, MaaNodeId id);

    std::optional<MAA_TASK_NS::RecoResult> get_reco_result(MaaRecoId uid) const;
    void set_reco_detail(MaaRecoId uid, MAA_TASK_NS::RecoResult);

    std::optional<MAA_TASK_NS::ActionResult> get_action_result(MaaActId uid) const;
    void set_action_detail(MaaActId uid, MAA_TASK_NS::ActionResult);

    std::optional<MAA_TASK_NS::WaitFreezesDetail> get_wf_detail(MaaWfId uid) const;
    void set_wf_detail(MaaWfId uid, MAA_TASK_NS::WaitFreezesDetail detail);

    std::optional<MAA_TASK_NS::NodeDetail> get_node_detail(MaaNodeId uid) const;
    void set_node_detail(MaaNodeId uid, MAA_TASK_NS::NodeDetail detail);

    std::optional<MAA_TASK_NS::TaskDetail> get_task_detail(MaaTaskId uid) const;
    void set_task_detail(MaaTaskId uid, MAA_TASK_NS::TaskDetail detail);

    // 每个任务只保留最近 kTaskNodeLimit 个节点的详情，及这些节点期间产生的识别、动作、wait_freezes 详情和子任务详情
    // Each task keeps the details of its last kTaskNodeLimit nodes, and the recognition, action, wait_freezes and
    // subtask details made during them (MaaXYZ/MaaFramework#1578)
    void own_task_node(MaaTaskId task_id, MaaNodeId node_id);
    void own_subtask(MaaTaskId subtask_id);

    // ponytail: fixed cap, a global option like reco_image_cache_limit if a client needs another value
    static constexpr size_t kTaskNodeLimit = 200;

    void clear();

private:
    // ids set since the last node a task owned
    struct OwnedIds
    {
        std::vector<MaaNodeId> nodes;
        std::vector<MaaRecoId> recos;
        std::vector<MaaActId> actions;
        std::vector<MaaWfId> wfs;
        std::vector<MaaTaskId> subtasks;
    };

    struct RetainedNode
    {
        MaaNodeId node_id = MaaInvalidId;
        MaaRecoId reco_id = MaaInvalidId;
    };

    void erase_owned(const OwnedIds& ids);
    void erase_node_and_reco(MaaNodeId node_id, MaaRecoId reco_id);

    struct RecoImageCache
    {
        MAA_TASK_NS::ImageEncodedBuffer raw;
        std::vector<MAA_TASK_NS::ImageEncodedBuffer> draws;
    };

    void evict_reco_image_cache_if_needed(size_t limit);

    std::map<std::string, MaaNodeId> latest_nodes_;
    // a dropped node that is still the latest of its name, kept until the name runs again (roi target, get_latest_node)
    std::map<std::string, RetainedNode> retained_nodes_;
    mutable std::shared_mutex latest_nodes_mutex_;

    OwnedIds unowned_;
    std::unordered_map<MaaTaskId, std::deque<std::pair<MaaNodeId, OwnedIds>>> task_nodes_;
    std::mutex owner_mutex_;

    std::map<MaaRecoId, MAA_TASK_NS::RecoResult> reco_details_;
    std::unordered_map<MaaRecoId, RecoImageCache> reco_image_cache_;
    std::deque<MaaRecoId> reco_image_order_;
    mutable std::shared_mutex reco_details_mutex_;

    std::map<MaaActId, MAA_TASK_NS::ActionResult> action_details_;
    mutable std::shared_mutex action_details_mutex_;

    std::map<MaaWfId, MAA_TASK_NS::WaitFreezesDetail> wf_details_;
    mutable std::shared_mutex wf_details_mutex_;

    std::map<MaaNodeId, MAA_TASK_NS::NodeDetail> node_details_;
    mutable std::shared_mutex node_details_mutex_;

    std::map<MaaTaskId, MAA_TASK_NS::TaskDetail> task_details_;
    mutable std::shared_mutex task_details_mutex_;
};

MAA_NS_END
