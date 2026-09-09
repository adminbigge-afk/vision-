# 列表模型（第四阶段）

v1.0 的播放队列由 MainWindow 中的 QStringList 与 QListWidget 维护；历史和收藏由 Library 查询。当前规模不单独建立空的 PlaylistModel 抽象。
