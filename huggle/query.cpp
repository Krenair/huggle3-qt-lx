//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "query.h"

using namespace Huggle;

unsigned int Query::LastID = 0;
QNetworkAccessManager Query::NetworkManager;

Query::Query()
{
    this->LockingThread = NULL;
    this->Result = NULL;
    this->Type = QueryNull;
    this->Status = StatusNull;
    this->ID = this->LastID;
    this->LastID++;
    this->CustomStatus = "";
    this->callback = NULL;
    this->HiddenQuery = false;
    this->Dependency = NULL;
    this->Managed = false;
    this->CallbackResult = NULL;
}

Query::~Query()
{
    if (this->Managed)
    {
        throw new Exception("Request to delete managed query");
    }
    delete Result;
    if (this->CallbackResult != NULL)
    {
        throw new Exception("Memory leak: Query::CallbackResult was not deleted before destructor was called");
    }
    this->Result = NULL;
}

bool Query::Processed()
{
    if (this->Status == StatusDone || this->Status == StatusInError)
    {
        return true;
    }
    return false;
}

bool Query::IsLocked()
{
    return Locked;
}

void Query::Lock()
{
    InternalLock.lock();
    if (this->LockingThread == QThread::currentThread())
    {
        // if current query is locked by same thread, we can safely
        // ignore it
        InternalLock.unlock();
        return;
    }
    this->QL.lock();
    this->LockingThread = QThread::currentThread();
    Locked = true;
    InternalLock.unlock();
}

void Query::Unlock()
{
    InternalLock.lock();
    if (!Locked)
    {
        InternalLock.unlock();
        return;
    }
    this->QL.unlock();
    this->LockingThread = NULL;
    Locked = false;
    InternalLock.unlock();
}

bool Query::IsManaged()
{
    if (this->Managed)
    {
        return true;
    }
    return (this->Consumers.count() > 0);
}

QString Query::QueryTypeToString()
{
    switch (this->Type)
    {
        case QueryNull:
            return "null";
        case QueryWl:
            return "Wl Query";
        case QueryApi:
            return "Api Query";
        case QueryRevert:
            return "Revert Query";
        case QueryEdit:
            return "Edit Query";
    }
    return "Unknown";
}

QString Query::QueryTargetToString()
{
    return "Invalid target";
}

QString Query::QueryStatusToString()
{
    if (this->CustomStatus != "")
    {
        return CustomStatus;
    }

    switch (this->Status)
    {
    case StatusNull:
        return "NULL";
    case StatusDone:
        return "Done";
    case StatusProcessing:
        return "Processing";
    case StatusInError:
        return "InError";
    }
    return "Unknown";
}

void Query::ProcessCallback()
{
    if (this->callback != NULL)
    {
        this->RegisterConsumer("delegate");
        this->CallbackResult = this->callback(this);
    }
}

bool Query::SafeDelete()
{
    if (Consumers.count() == 0)
    {
        QueryGC::Lock.lock();
        if (QueryGC::qgc.contains(this))
        {
            QueryGC::qgc.removeAll(this);
        }
        QueryGC::Lock.unlock();
        this->Managed = false;
        delete this;
        return true;
    }

    this->SetManaged();
    return false;
}

void Query::RegisterConsumer(QString consumer)
{
    this->Lock();
    this->Consumers.append(consumer);
    this->Consumers.removeDuplicates();
    this->SetManaged();
    this->Unlock();
}

void Query::UnregisterConsumer(QString consumer)
{
    this->Lock();
    this->Consumers.removeAll(consumer);
    this->SetManaged();
    this->Unlock();
}

QString Query::DebugQgc()
{
    QString result = "";
    if (this->Consumers.count() > 0)
    {
        result += ("GC: Listing all dependencies for " + QString::number(this->QueryID())) + "\n";
        int Item=0;
        while (Item < this->Consumers.count())
        {
            result +=("GC: " + QString::number(this->QueryID()) + " " + this->Consumers.at(Item)) + "\n";
            Item++;
        }
    } else
    {
        result += "No consumers found: " + QString::number(this->QueryID());
    }
    return result;
}

unsigned int Query::QueryID()
{
    return this->ID;
}

void Query::SetManaged()
{
    this->Managed = true;
    if (!QueryGC::qgc.contains(this))
    {
        QueryGC::qgc.append(this);
    }
}
