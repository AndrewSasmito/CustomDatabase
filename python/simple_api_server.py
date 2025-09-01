#!/usr/bin/env python3
"""
Simple FastAPI server for demonstration
"""
import asyncio
import logging
from typing import Optional, Dict, Any
import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.responses import Response
from pydantic import BaseModel
from prometheus_client import Counter, Histogram, Gauge, generate_latest, CONTENT_TYPE_LATEST

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Simple in-memory storage for demonstration
memory_store = {}
transaction_counter = 1

# Prometheus metrics
db_operations_total = Counter('db_operations_total', 'Total database operations', ['operation', 'status'])
db_operation_duration = Histogram('db_operation_duration_seconds', 'Database operation duration', ['operation'])
db_health_status = Gauge('db_health_status', 'Database health status (1=healthy, 0=unhealthy)')
db_total_keys = Gauge('db_total_keys', 'Total number of keys in database')
db_memory_usage = Gauge('db_memory_usage_bytes', 'Memory usage in bytes')

# FastAPI app
app = FastAPI(
    title="Custom Database Engine API",
    description="High-performance database (Demo Mode)",
    version="1.0.0",
)

# Pydantic stuff
class InsertRequest(BaseModel):
    key: str
    value: str

class SearchResponse(BaseModel):
    key: str
    value: Optional[str]
    found: bool

class StatsResponse(BaseModel):
    total_keys: int
    memory_usage: str
    system_healthy: bool

# API Endpoints
@app.get("/")
async def root():
    """Root endpoint with API information"""
    return {
        "message": "Custom Database Engine API (Demo Mode)",
        "version": "1.0.0",
        "status": "healthy",
        "mode": "demo",
        "features": [
            "Content-addressable storage",
            "B-Tree indexing", 
            "MVCC transactions",
            "Health monitoring",
            "FastAPI REST interface"
        ],
        "note": "Currently running in demo mode without C++ engine"
    }

@app.post("/insert", response_model=dict)
async def insert_key_value(request: InsertRequest):
    """Insert a key-value pair into the database"""
    with db_operation_duration.labels(operation='insert').time():
        try:
            memory_store[request.key] = request.value
            db_operations_total.labels(operation='insert', status='success').inc()
            db_total_keys.set(len(memory_store))
            db_memory_usage.set(sum(len(str(k)) + len(str(v)) for k, v in memory_store.items()))
            logger.info(f"Inserted: {request.key} -> {request.value}")
            return {"success": True, "key": request.key}
        except Exception as e:
            db_operations_total.labels(operation='insert', status='error').inc()
            raise HTTPException(status_code=500, detail=f"Insert failed: {str(e)}")

@app.get("/search/{key}", response_model=SearchResponse)
async def search_key(key: str):
    """Search for a key in the database"""
    with db_operation_duration.labels(operation='search').time():
        try:
            value = memory_store.get(key)
            found = value is not None
            db_operations_total.labels(operation='search', status='success').inc()
            logger.info(f"Search: {key} -> {'found' if found else 'not found'}")
            return SearchResponse(
                key=key,
                value=value if found else None,
                found=found
            )
        except Exception as e:
            db_operations_total.labels(operation='search', status='error').inc()
            raise HTTPException(status_code=500, detail=f"Search failed: {str(e)}")

@app.delete("/remove/{key}")
async def remove_key(key: str):
    """Remove a key from the database"""
    with db_operation_duration.labels(operation='remove').time():
        try:
            if key in memory_store:
                del memory_store[key]
                db_operations_total.labels(operation='remove', status='success').inc()
                db_total_keys.set(len(memory_store))
                db_memory_usage.set(sum(len(str(k)) + len(str(v)) for k, v in memory_store.items()))
                logger.info(f"Removed: {key}")
                return {"success": True, "key": key}
            else:
                db_operations_total.labels(operation='remove', status='not_found').inc()
                return {"success": False, "key": key, "message": "Key not found"}
        except Exception as e:
            db_operations_total.labels(operation='remove', status='error').inc()
            raise HTTPException(status_code=500, detail=f"Remove failed: {str(e)}")

@app.get("/stats", response_model=StatsResponse)
async def get_stats():
    """Get database system statistics"""
    try:
        total_size = sum(len(str(k)) + len(str(v)) for k, v in memory_store.items())
        return StatsResponse(
            total_keys=len(memory_store),
            memory_usage=f"{total_size} bytes",
            system_healthy=True
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Stats failed: {str(e)}")

@app.get("/health")
async def health_check():
    """Health check endpoint"""
    # Update health metrics
    db_health_status.set(1)  # 1 = healthy
    db_total_keys.set(len(memory_store))
    db_memory_usage.set(sum(len(str(k)) + len(str(v)) for k, v in memory_store.items()))
    
    return {
        "status": "healthy",
        "database": "demo_mode",
        "timestamp": asyncio.get_event_loop().time(),
        "keys_stored": len(memory_store)
    }

@app.post("/transactions/begin")
async def begin_transaction():
    """Begin a new database transaction (demo)"""
    global transaction_counter
    txn_id = transaction_counter
    transaction_counter += 1
    logger.info(f"Started transaction {txn_id}")
    return {"transaction_id": txn_id}

@app.post("/transactions/{txn_id}/commit")
async def commit_transaction(txn_id: int):
    """Commit a database transaction (demo)"""
    logger.info(f"Committed transaction {txn_id}")
    return {"success": True, "transaction_id": txn_id}

@app.post("/transactions/{txn_id}/abort")
async def abort_transaction(txn_id: int):
    """Abort a database transaction (demo)"""
    logger.info(f"Aborted transaction {txn_id}")
    return {"success": True, "transaction_id": txn_id}

@app.post("/admin/flush")
async def flush_database():
    """Flush pending writes (demo operation)"""
    logger.info("Flush operation completed (demo mode)")
    return {"message": "Flush completed (demo mode)"}

@app.get("/demo/populate")
async def populate_demo_data():
    """Populate some demo data for testing"""
    demo_data = {
        "user:1": "Alice Johnson",
        "user:2": "Bob Smith", 
        "user:3": "Carol Davis",
        "product:1": "Database Engine",
        "product:2": "FastAPI Server",
        "config:max_connections": "1000",
        "config:timeout": "30s"
    }
    
    memory_store.update(demo_data)
    logger.info(f"Populated {len(demo_data)} demo records")
    
    return {
        "message": f"Populated {len(demo_data)} demo records",
        "keys": list(demo_data.keys())
    }

@app.delete("/demo/clear")
async def clear_demo_data():
    """Clear all demo data"""
    count = len(memory_store)
    memory_store.clear()
    db_total_keys.set(0)
    db_memory_usage.set(0)
    logger.info(f"Cleared {count} records")
    
    return {
        "message": f"Cleared {count} records",
        "total_keys": len(memory_store)
    }

@app.get("/metrics")
async def get_metrics():
    """Prometheus metrics endpoint"""
    return Response(generate_latest(), media_type=CONTENT_TYPE_LATEST)

if __name__ == "__main__":
    print("Starting FastAPI Database Server (Demo Mode)")
    print("API Documentation: http://localhost:8000/docs")
    print("Health Check: http://localhost:8000/health")
    print("Prometheus Metrics: http://localhost:8000/metrics")
    print("Populate Demo Data: http://localhost:8000/demo/populate")
    print("=" * 60)
    
    uvicorn.run(
        "simple_api_server:app",
        host="0.0.0.0",
        port=8000,
        reload=True,
        log_level="info"
    )
