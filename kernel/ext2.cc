#include "ext2.h"
#include "libk.h"
#include "utils.h"
#include "path.h"


Ext2::Ext2(StrongPtr<Ide> ide_): ide_(ide_), root() {
    using namespace impl::ext2;

    SuperBlock sb;

    ide_->read(1024,sb);
    iNodeSize = sb.inode_size;
    iNodesPerGroup = sb.inodes_per_group;
    numberOfNodes = sb.inodes_count;
    numberOfBlocks = sb.blocks_count;

    //Debug::printf("inodes_count %d\n",sb.inodes_count);
    //Debug::printf("blocks_count %d\n",sb.blocks_count);
    //Debug::printf("blocks_per_group %d\n",sb.blocks_per_group);
    //Debug::printf("inode_size %d\n",sb.inode_size);
    //Debug::printf("block_group_nr %d\n",sb.block_group_nr);
    //Debug::printf("first_inode %d\n",sb.first_inode);

    blockSize = uint32_t(1) << (sb.log_block_size + 10);

    buffer_cache = StrongPtr<BufferCache>::make(ide_);
    
    nGroups = (sb.blocks_count + sb.blocks_per_group - 1) / sb.blocks_per_group;
    //Debug::printf("nGroups = %d\n",nGroups);
    ASSERT(nGroups * sb.blocks_per_group >= sb.blocks_count);

    auto superBlockNumber = 1024 / blockSize;
    //Debug::printf("super block number %d\n",superBlockNumber);

    auto groupTableNumber = superBlockNumber + 1;
    //Debug::printf("group table number %d\n",groupTableNumber);

    auto groupTableSize = sizeof(BlockGroup) * nGroups;
    //Debug::printf("group table size %d\n",groupTableSize);

    auto groupTable = new BlockGroup[nGroups];
    auto cnt = buffer_cache->read_all(groupTableNumber * blockSize, groupTableSize, (char*) groupTable);
    ASSERT(cnt == groupTableSize);

    iNodeTables = new uint32_t[nGroups];

    for (uint32_t i=0; i < nGroups; i++) {
        auto g = &groupTable[i];

        iNodeTables[i] = g->inode_table;

        //Debug::printf("group #%d\n",i);
        //Debug::printf("    block_bitmap %d\n",g->block_bitmap);
        //Debug::printf("    inode_bitmap %d\n",g->inode_bitmap);
        //Debug::printf("    inode_table %d\n",g->inode_table);

    }

    //Debug::printf("========\n");
    //Debug::printf("iNodeSize %d\n",iNodeSize);
    //Debug::printf("nGroups %d\n",nGroups);
    //Debug::printf("iNodesPerGroup %d\n",iNodesPerGroup);
    //Debug::printf("numberOfNodes %d\n",numberOfNodes);
    //Debug::printf("numberOfBlocks %d\n",numberOfBlocks);
    //Debug::printf("blockSize %d\n",blockSize);
    //for (unsigned i = 0; i < nGroups; i++) {
    //    Debug::printf("iNodeTable[%d] %d\n",i,iNodeTables[i]);
    //}

    //root = new Node(ide,2,blockSize);

    root = get_node(2);

    //root->show("root");

    //root->entries([](uint32_t inode, char* name) {
    //    Debug::printf("%d %s\n",inode,name);
    //});



    //println(sb.uuid);
    //println(sb.volume_name);
}

StrongPtr<Node> Ext2::get_node(uint32_t number) {
    if (number == 0) return {};
    ASSERT(number <= numberOfNodes);
    auto index = number - 1;

    auto groupIndex = index / iNodesPerGroup;
    //Debug::printf("groupIndex %d\n",groupIndex);
    ASSERT(groupIndex < nGroups);
    auto indexInGroup = index % iNodesPerGroup;
    auto iTableBase = iNodeTables[groupIndex];
    ASSERT(iTableBase <= numberOfBlocks);
    //Debug::printf("iTableBase %d\n",iTableBase);
    auto nodeOffset = iTableBase * blockSize + indexInGroup * iNodeSize;
    //Debug::printf("nodeOffset %d\n",nodeOffset);

    auto out = StrongPtr<Node>::make(buffer_cache,number,blockSize);
    buffer_cache->read(nodeOffset,out->data);
    return out;
}

StrongPtr<Node> Ext2::find_one(StrongPtr<Node> dir, char* name) {
    uint32_t full_offset = 0;
    while(full_offset < (uint32_t)dir->size_in_bytes()) {
        uint32_t inode_id;
        dir->read<uint32_t>(full_offset + 0, inode_id);

        uint16_t offset;
        dir->read<uint16_t>(full_offset + 4, offset);

        uint8_t name_length;
        dir->read<uint8_t>(full_offset + 6, name_length);
        
        uint8_t i = 0;
        while(true) {
            char c = 0;
            if(i != name_length) {
                dir->read<char>(full_offset + 8 + i, c);
            }
            // Debug::printf("%c", c);
            if (c != name[i]) {
                // Debug::printf(" - NOT THE SAME: %d != %d\n", c, name[i]);
                break; // Not the same name
            }
            // found it
            if (i++ >= name_length) {
                return get_node(inode_id);
            }
        }
        full_offset += offset;
    }
    return StrongPtr<Node>{};
}

StrongPtr<Node> Ext2::find(StrongPtr<Node> dir, const char* name) {
    // Debug::printf("BEGINNING OF FIND %s\n", name);
    
    Path path{name};
    StrongPtr<PathString> curr = path.remove();
    uint32_t count = 0;
    
    StrongPtr<Node> node = dir;
    
    while(!(curr == nullptr)) {
        if(count >= 100) return StrongPtr<Node>{}; // break cycles
        
        char* str = curr->get();
        // Debug::printf("HERE 1 %s | %s\n", name, str);
        if(str[0] == '/') {
            // Debug::printf("HERE 2.1 %s | %s\n", name, str);
            node = root;
        } else {
            // Debug::printf("HERE 2.2 %s | %s\n", name, str);
            if(!node->is_dir()) return StrongPtr<Node>{};
            StrongPtr<Node> next_node = find_one(node, str);
            // Debug::printf("HERE 2.22 %s | %s\n", name, str);
            if(next_node == nullptr) return StrongPtr<Node>();
            
            if(next_node->is_symlink()) {
                count++;
            }

            // Symlink
            if(!path.isEmpty() && next_node->is_symlink()) {
                char new_path_str[next_node->size_in_bytes() + 1];
                next_node->get_symbol((char*)&new_path_str);
                Path new_path{new_path_str};
                path.merge_symbolic_link(new_path);
                new_path.clear();
            } 
            // File / Dir
            else {
                // Debug::printf("HERE 3 %s | %s\n", name, str);
                node = next_node;
            }

        }

        delete[] str;
        curr = path.remove();
    }

    // Debug::printf("END OF FIND %s\n", name);

    return node;
}


////////////// NodeData //////////////

void NodeData::show(const char* what) {
    Debug::printf("%s\n",what);
    Debug::printf("    mode 0x%x\n",mode);
    Debug::printf("    uid %d\n",uid);
    Debug::printf("    gif %d\n",gid);
    Debug::printf("    n_links %d\n",n_links);
    Debug::printf("    n_sectors %d\n",n_sectors);
}

///////////// Node /////////////

void Node::get_symbol(char* buffer) {
    ASSERT(is_symlink());
    auto sz = size_in_bytes();
    if (sz < 60) {
        memcpy(buffer,&data.direct0,sz);
    } else {
        auto cnt = read_all(0,sz,buffer);
        ASSERT(cnt == sz);
    }
}

void Node::read_block(uint32_t lbn, char* buffer) {
    auto refs_per_block = block_size / 4;

    // follow one level of indirection
    auto follow([this, refs_per_block](uint32_t pbn, uint32_t index) -> uint32_t {
        ASSERT(index < refs_per_block);
        if (pbn == 0) return 0;

        uint32_t out;
        buffer_cache->read(pbn * block_size + index * 4, out);
        return out;
    });

    uint32_t pbn = 0;

    if (lbn < 12) {
        uint32_t* direct = &data.direct0;
        pbn = direct[lbn];
    } else {
        lbn -= 12;
        if (lbn < refs_per_block) {
            pbn = follow(data.indirect_1, lbn); 
        } else {
            lbn -= refs_per_block;
            if (lbn < (refs_per_block * refs_per_block)) {
                auto d1 = lbn / refs_per_block;
                auto d0 = lbn % refs_per_block;
                pbn = follow(follow(data.indirect_2, d1), d0);
            } else {
                lbn -= refs_per_block * refs_per_block;
                auto d0 = lbn % refs_per_block;
                auto t = lbn / refs_per_block;
                auto d1 = t % refs_per_block;
                auto d2 = t / refs_per_block;
                ASSERT(d2 < refs_per_block);
                ASSERT((((d2 * refs_per_block + d1) * refs_per_block) + d0) == lbn);
                pbn = follow(follow(follow(data.indirect_3, d2), d1), d0);
            }
        }
    }

    if (pbn == 0) {
        // zero-filled, sparse
        bzero(buffer, block_size);
    } else {
        auto cnt = buffer_cache->read_all(pbn * block_size, block_size,buffer);
        ASSERT(cnt == block_size);
    }
}

uint32_t Node::entry_count() {
    ASSERT(is_dir());
    uint32_t count = 0;
    entries([&count](uint32_t,const char*,uint32_t) -> uint32_t {
        count += 1;
        return 0;
    });
    return count;
}

